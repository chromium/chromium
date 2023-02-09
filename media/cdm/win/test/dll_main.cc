// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <mfapi.h>
#include <wrl/module.h>

#include "base/logging.h"
#include "media/base/win/mf_helpers.h"

using Microsoft::WRL::InProc;
using Microsoft::WRL::Module;

BOOL WINAPI DllMain(_In_opt_ HINSTANCE instance,
                    _In_ DWORD reason,
                    _In_opt_ LPVOID reserved) {
  if (DLL_PROCESS_ATTACH == reason) {
    // Don't need per-thread callbacks
    DisableThreadLibraryCalls(instance);

    Module<InProc>::GetModule().Create();

    HRESULT hr = MFStartup(MF_VERSION);
    if (!SUCCEEDED(hr)) {
      DVLOG(1) << "Failed with HRESULT=" << media::PrintHr(hr);
      return FALSE;
    }
  } else if (DLL_PROCESS_DETACH == reason) {
    Module<InProc>::GetModule().Terminate();
  }

  return TRUE;
}

HRESULT WINAPI
DllGetActivationFactory(_In_ HSTRING activatible_class_id,
                        _COM_Outptr_ IActivationFactory** factory) {
  auto& module = Module<InProc>::GetModule();
  return module.GetActivationFactory(activatible_class_id, factory);
}

HRESULT WINAPI DllCanUnloadNow() {
  auto& module = Module<InProc>::GetModule();
  return (module.Terminate()) ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(_In_ REFCLSID rclsid,
                         _In_ REFIID riid,
                         _COM_Outptr_ LPVOID FAR* ppv) {
  auto& module = Module<InProc>::GetModule();
  return module.GetClassObject(rclsid, riid, ppv);
}
