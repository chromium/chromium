// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_PROCESS_MITIGATIONS_WIN32K_POLICY_H_
#define SANDBOX_WIN_SRC_PROCESS_MITIGATIONS_WIN32K_POLICY_H_

#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/policy_low_level.h"
#include "sandbox/win/src/sandbox_policy.h"

namespace sandbox {

// This class centralizes most of the knowledge related to the process
// mitigations Win32K lockdown policy.
class ProcessMitigationsWin32KLockdownPolicy {
 public:
  // Creates the required low-level policy rules to evaluate a high-level
  // policy rule for the Win32K process mitigation policy.
  // name is the object name, semantics is the desired semantics for the
  // open or create and policy is the policy generator to which the rules are
  // going to be added.
  static bool GenerateRules(const wchar_t* name,
                            Semantics semantics,
                            LowLevelPolicy* policy);
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_PROCESS_MITIGATIONS_WIN32K_POLICY_H_
