// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SERVICES_YAMA_H_
#define SANDBOX_LINUX_SERVICES_YAMA_H_

#include "sandbox/sandbox_export.h"

namespace sandbox {

// Yama is a LSM kernel module which can restrict ptrace().
// This class provides ways to detect if Yama is present and enabled
// and to restrict which processes can ptrace the current process.
class SANDBOX_EXPORT Yama {
 public:
  // This enum should be used to set or check a bitmask.
  // A value of 0 would indicate that the status is not known.
  enum GlobalStatus {
    STATUS_KNOWN = 1 << 0,
    STATUS_PRESENT = 1 << 1,
    STATUS_ENFORCING = 1 << 2,
    // STATUS_STRICT_ENFORCING corresponds to either mode 2 or mode 3 of Yama.
    // Ptrace could be entirely denied, or restricted to CAP_SYS_PTRACE
    // and PTRACE_TRACEME.
    STATUS_STRICT_ENFORCING = 1 << 3
  };

  Yama() = delete;
  Yama(const Yama&) = delete;
  Yama& operator=(const Yama&) = delete;

  // Restrict who can ptrace() the current process to its ancestors.
  // If this succeeds, then Yama is available on this kernel.
  // However, Yama may not be enforcing at this time.
  static bool RestrictPtracersToAncestors();

  // Disable Yama restrictions for the current process.
  // This will fail if Yama is not available on this kernel.
  // This is meant for testing only. If you need this, implement
  // a per-pid authorization instead.
  static bool DisableYamaRestrictions();

  // Checks if Yama is currently in enforcing mode for the machine (not the
  // current process). This requires access to the filesystem and will use
  // /proc/sys/kernel/yama/ptrace_scope.
  static int GetStatus();

  // Helper for checking for STATUS_PRESENT in GetStatus().
  static bool IsPresent();
  // Helper for checkking for STATUS_ENFORCING in GetStatus().
  static bool IsEnforcing();
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SERVICES_YAMA_H_
