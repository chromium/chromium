// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/process_mitigations_win32k_policy.h"

#include <stddef.h>

#include "sandbox/win/src/process_mitigations_win32k_interception.h"

namespace sandbox {

bool ProcessMitigationsWin32KLockdownPolicy::GenerateRules(
    LowLevelPolicy* policy) {
  PolicyRule rule(FAKE_SUCCESS);
  if (!policy->AddRule(IpcTag::GDI_GDIDLLINITIALIZE, &rule))
    return false;
  if (!policy->AddRule(IpcTag::GDI_GETSTOCKOBJECT, &rule))
    return false;
  if (!policy->AddRule(IpcTag::USER_REGISTERCLASSW, &rule))
    return false;
  return true;
}

}  // namespace sandbox
