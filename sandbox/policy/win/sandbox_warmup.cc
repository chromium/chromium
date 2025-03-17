// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/win/sandbox_warmup.h"

#include <windows.h>

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "sandbox/policy/win/hook_util/hook_util.h"

// Prototype for ProcessPrng.
// See: https://learn.microsoft.com/en-us/windows/win32/seccng/processprng
extern "C" {
BOOL WINAPI ProcessPrng(PBYTE pbData, SIZE_T cbData);
}

namespace sandbox::policy {

namespace {

// Stores the default lcid.
LCID g_user_default_lcid = 0;

// Returns the lcid seen at startup.
LCID HookedGetUserDefaultLCID() {
  return g_user_default_lcid;
}

// Import bcryptprimitives!ProcessPrng rather than cryptbase!RtlGenRandom to
// avoid opening a handle to \\Device\KsecDD in the renderer.
decltype(&ProcessPrng) GetProcessPrng() {
  HMODULE hmod = LoadLibraryW(L"bcryptprimitives.dll");
  CHECK(hmod);
  decltype(&ProcessPrng) process_prng_fn =
      reinterpret_cast<decltype(&ProcessPrng)>(
          GetProcAddress(hmod, "ProcessPrng"));
  CHECK(process_prng_fn);
  return process_prng_fn;
}

}  // namespace

void WarmupRandomnessInfrastructure() {
  BYTE data[1];
  // TODO(crbug.com/40088338) Call a warmup function exposed by boringssl.
  static decltype(&ProcessPrng) process_prng_fn = GetProcessPrng();
  BOOL success = process_prng_fn(data, sizeof(data));
  // ProcessPrng is documented to always return TRUE.
  CHECK(success);
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

}  // namespace sandbox::policy
