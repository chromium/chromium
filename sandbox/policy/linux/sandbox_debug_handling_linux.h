// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_LINUX_SANDBOX_DEBUG_HANDLING_LINUX_H_
#define SANDBOX_POLICY_LINUX_SANDBOX_DEBUG_HANDLING_LINUX_H_

#include "sandbox/policy/export.h"

namespace sandbox {
namespace policy {

class SANDBOX_POLICY_EXPORT SandboxDebugHandling {
 public:
  SandboxDebugHandling() = delete;
  SandboxDebugHandling(const SandboxDebugHandling&) = delete;
  SandboxDebugHandling& operator=(const SandboxDebugHandling&) = delete;

  // Depending on the command line, set the current process as
  // non dumpable. Also set any signal handlers for sandbox
  // debugging.
  static bool SetDumpableStatusAndHandlers();
};

}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_LINUX_SANDBOX_DEBUG_HANDLING_LINUX_H_
