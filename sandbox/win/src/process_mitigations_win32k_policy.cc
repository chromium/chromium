// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/process_mitigations_win32k_policy.h"

#include <stddef.h>

#include "sandbox/win/src/process_mitigations_win32k_interception.h"

namespace sandbox {

bool ProcessMitigationsWin32KLockdownPolicy::GenerateRules(
    LowLevelPolicy* policy) {
  return policy->AddRule(IpcTag::GDI_GDIDLLINITIALIZE,
                         PolicyRule{FAKE_SUCCESS}) &&
         policy->AddRule(IpcTag::GDI_GETSTOCKOBJECT,
                         PolicyRule{FAKE_SUCCESS}) &&
         policy->AddRule(IpcTag::USER_REGISTERCLASSW, PolicyRule{FAKE_SUCCESS});
}

}  // namespace sandbox
