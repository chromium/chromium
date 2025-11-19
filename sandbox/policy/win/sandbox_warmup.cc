// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/win/sandbox_warmup.h"

#include <windows.h>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/win/delayload_helpers.h"
#include "sandbox/policy/win/hook_util/hook_util.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox::policy {

namespace {

// TODO(crbug.com/443286263) allows symbolizations inside sandboxes to be
// tracked down and fixed. When enabled this prevents prewarming dbghelp.dll
// before entering lockdown.
BASE_FEATURE(kWinSboxNoDbghelpWarmup, base::FEATURE_ENABLED_BY_DEFAULT);

// Stores the default lcid.
LCID g_user_default_lcid = 0;

// Returns the lcid seen at startup.
LCID HookedGetUserDefaultLCID() {
  return g_user_default_lcid;
}

}  // namespace

void WarmupRandomnessInfrastructure() {
  sandbox::WarmupRandomnessInfrastructure();
}

bool HookDwriteGetUserDefaultLCID() {
  // Should not be called twice.
  static bool first_call = true;
  CHECK(first_call);
  first_call = false;

  // In Chrome dwrite.dll should be loaded as it is an import of chrome.dll.
  HMODULE h_dwrite = ::GetModuleHandleW(L"dwrite.dll");
  if (!h_dwrite) {
    return false;
  }

  // Store this for our hook to return if it is called.
  g_user_default_lcid = GetUserDefaultLCID();

  // We never call the original version of GetUserDefaultLCID and never unhook.
  static base::NoDestructor<sandbox::policy::IATHook> dwrite_hook;

  // dwrite.dll imports 1-2-1 in Windows 10 10240 & 1-2-0 in Windows 11 22631.
  const char* apisets[] = {"api-ms-win-core-localization-l1-2-0.dll",
                           "api-ms-win-core-localization-l1-2-1.dll",
                           "api-ms-win-core-localization-l1-2-2.dll"};
  for (const char* apiset : apisets) {
    if (NO_ERROR ==
        dwrite_hook->Hook(h_dwrite, apiset, "GetUserDefaultLCID",
                          reinterpret_cast<void*>(HookedGetUserDefaultLCID))) {
      return true;
    }
  }
  return false;
}

void MaybeDelayloadDbghelp() {
  if (!base::FeatureList::IsEnabled(kWinSboxNoDbghelpWarmup)) {
    std::ignore = base::win::LoadAllImportsForDll("dbghelp.dll");
  }
}

}  // namespace sandbox::policy
