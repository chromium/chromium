/* Copyright 2024 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Landlock system definitions.
 *
 * These definitions are based on <linux/landlock.h>. However, because we
 * can't guarantee that header will be available on all systems, they are
 * extracted here. We only include definitions needed for checking the Landlock
 * version, as we just need to determine if the system supports Landlock.
 */

#ifndef SANDBOX_LINUX_SYSTEM_HEADERS_LANDLOCK_H_
#define SANDBOX_LINUX_SYSTEM_HEADERS_LANDLOCK_H_

#include <stddef.h>
#include <stdint.h>

/**
 * struct landlock_ruleset_attr - Ruleset definition
 *
 * Argument of sys_landlock_create_ruleset().
 */
struct landlock_ruleset_attr {
  /**
   * @handled_access_fs: Bitmask of actions (cf. `Filesystem flags`_)
   * that is handled by this ruleset and should then be forbidden if no
   * rule explicitly allow them.  This is needed for backward
   * compatibility reasons.
   */
  uint64_t handled_access_fs;
};

/*
 * sys_landlock_create_ruleset() flags:
 *
 * - %LANDLOCK_CREATE_RULESET_VERSION: Get the highest supported Landlock ABI
 *   version.
 */
#ifndef LANDLOCK_CREATE_RULESET_VERSION
#define LANDLOCK_CREATE_RULESET_VERSION (1U << 0)
#endif

// Syscall number for landlock_create_ruleset taken from <asm-generic/unistd.h>.
#ifndef __NR_landlock_create_ruleset
#define __NR_landlock_create_ruleset 444
#endif

#endif  // SANDBOX_LINUX_SYSTEM_HEADERS_LANDLOCK_H_
