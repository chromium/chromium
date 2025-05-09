// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Landlock functions and constants.

#ifndef SANDBOX_POLICY_LINUX_LANDLOCK_GPU_POLICY_ANDROID_H_
#define SANDBOX_POLICY_LINUX_LANDLOCK_GPU_POLICY_ANDROID_H_

#include "sandbox/linux/services/syscall_wrappers.h"
#include "sandbox/linux/system_headers/linux_landlock.h"
#include "sandbox/policy/export.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"

namespace sandbox::landlock {

#define LANDLOCK_ACCESS_FS_ROUGHLY_READ \
  (LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR)

#define LANDLOCK_ACCESS_FS_ROUGHLY_READ_EXECUTE                \
  (LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_READ_FILE | \
   LANDLOCK_ACCESS_FS_READ_DIR)

#define LANDLOCK_ACCESS_FS_ROUGHLY_BASIC_WRITE                     \
  (LANDLOCK_ACCESS_FS_WRITE_FILE | LANDLOCK_ACCESS_FS_REMOVE_DIR | \
   LANDLOCK_ACCESS_FS_REMOVE_FILE | LANDLOCK_ACCESS_FS_MAKE_DIR |  \
   LANDLOCK_ACCESS_FS_MAKE_REG)

#define LANDLOCK_ACCESS_FS_ROUGHLY_EDIT                            \
  (LANDLOCK_ACCESS_FS_WRITE_FILE | LANDLOCK_ACCESS_FS_REMOVE_DIR | \
   LANDLOCK_ACCESS_FS_REMOVE_FILE)

#define LANDLOCK_ACCESS_FS_ROUGHLY_FULL_WRITE                      \
  (LANDLOCK_ACCESS_FS_WRITE_FILE | LANDLOCK_ACCESS_FS_REMOVE_DIR | \
   LANDLOCK_ACCESS_FS_REMOVE_FILE | LANDLOCK_ACCESS_FS_MAKE_CHAR | \
   LANDLOCK_ACCESS_FS_MAKE_DIR | LANDLOCK_ACCESS_FS_MAKE_REG |     \
   LANDLOCK_ACCESS_FS_MAKE_SOCK | LANDLOCK_ACCESS_FS_MAKE_FIFO |   \
   LANDLOCK_ACCESS_FS_MAKE_BLOCK | LANDLOCK_ACCESS_FS_MAKE_SYM)

#define LANDLOCK_ACCESS_FILE                                    \
  (LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_WRITE_FILE | \
   LANDLOCK_ACCESS_FS_READ_FILE)

#define LANDLOCK_HANDLED_ACCESS_TYPES        \
  (LANDLOCK_ACCESS_FS_ROUGHLY_READ_EXECUTE | \
   LANDLOCK_ACCESS_FS_ROUGHLY_FULL_WRITE)

// Applies a basic Landlock sandbox policy to the current process.
// Returns true if the policy was applied successfully, false otherwise.
// This function is a no-op and returns false on non-Android platforms.
SANDBOX_POLICY_EXPORT bool ApplyLandlock(mojom::Sandbox sandbox_type);

}  // namespace sandbox::landlock

#endif  // SANDBOX_POLICY_LINUX_LANDLOCK_GPU_POLICY_ANDROID_H_
