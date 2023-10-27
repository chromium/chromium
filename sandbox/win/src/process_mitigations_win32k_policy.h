// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_PROCESS_MITIGATIONS_WIN32K_POLICY_H_
#define SANDBOX_WIN_SRC_PROCESS_MITIGATIONS_WIN32K_POLICY_H_

#include "sandbox/win/src/policy_low_level.h"

namespace sandbox {

// This class centralizes most of the knowledge related to the process
// mitigations Win32K lockdown policy.
class ProcessMitigationsWin32KLockdownPolicy {
 public:
  // Creates the required low-level policy rules to evaluate a high-level
  // policy rule for the Win32K process mitigation policy.
  // `policy` is the policy generator to which the rules are
  // going to be added.
  static bool GenerateRules(LowLevelPolicy* policy);
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_PROCESS_MITIGATIONS_WIN32K_POLICY_H_
