// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/process_mitigations_win32k_dispatcher.h"

#include <algorithm>
#include <string>

#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/unguessable_token.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/interceptors.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/process_mitigations_win32k_interception.h"
#include "sandbox/win/src/process_mitigations_win32k_policy.h"

namespace sandbox {

ProcessMitigationsWin32KDispatcher::ProcessMitigationsWin32KDispatcher(
    PolicyBase* policy_base)
    : policy_base_(policy_base) {
}

ProcessMitigationsWin32KDispatcher::~ProcessMitigationsWin32KDispatcher() {}

bool ProcessMitigationsWin32KDispatcher::SetupService(
    InterceptionManager* manager,
    IpcTag service) {
  if (!(policy_base_->GetConfig()->GetProcessMitigations() &
        sandbox::MITIGATION_WIN32K_DISABLE)) {
    return false;
  }

  switch (service) {
    case IpcTag::GDI_GDIDLLINITIALIZE: {
      if (!INTERCEPT_EAT(manager, L"gdi32.dll", GdiDllInitialize,
                         GDIINITIALIZE_ID, 12)) {
        return false;
      }
      return true;
    }

    case IpcTag::GDI_GETSTOCKOBJECT: {
      if (!INTERCEPT_EAT(manager, L"gdi32.dll", GetStockObject,
                         GETSTOCKOBJECT_ID, 8)) {
        return false;
      }
      return true;
    }

    case IpcTag::USER_REGISTERCLASSW: {
      if (!INTERCEPT_EAT(manager, L"user32.dll", RegisterClassW,
                         REGISTERCLASSW_ID, 8)) {
        return false;
      }
      return true;
    }

    default:
      break;
  }
  return false;
}

}  // namespace sandbox
