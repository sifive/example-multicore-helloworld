/* Copyright 2019 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <stdlib.h>

#include <metal/cpu.h>
#include <metal/lock.h>

/* my_lock, when held, provides exclusive access to stdout and checkin_count */
METAL_LOCK_DECLARE(my_lock);

int main(void);

/* This is a count of the number of harts who have executed their main function.
   Writes to this variable must be protected by my_lock. */
volatile int checkin_count = 0;

/* Libc constructors run during _start while secondary harts are waiting for the boot hart
 * to finish initialization, so we can be sure that the lock is initialized before secondary
 * harts try to take it */
__attribute__((constructor))
void init_lock(void) {
	int rc = metal_lock_init(&my_lock);
	if(rc != 0) {
		puts("Failed to initialize my_lock\n");
		exit(1);
	}

	/* Ensure that the lock is initialized before any readers of the lock */
	__asm__ ("fence rw,w"); /* Release semantics */
}

/* The secondary_main() function can be redefined to start execution
 * on secondary harts. We redefine it here to cause all harts to execute main */
int secondary_main(void) {
	int rc = main();

	int hartid = metal_cpu_get_current_hartid();

	if(hartid == 0) {
		/* Return from hart 0 after all harts have run main */
		int num_harts = metal_cpu_get_num_harts();

		while(checkin_count != num_harts) ;

		return rc;
	} else {
		/* Spin secondary harts forever */
		while(1) {
			__asm__("wfi");
		}
	}
}

int main(void) {
	metal_lock_take(&my_lock);

	puts("hello world");
	fflush(stdout);

	checkin_count += 1;

	metal_lock_give(&my_lock);

	return 0;
}
