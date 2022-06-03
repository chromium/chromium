// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/default_apps_util.h"

#include <shobjidl.h>
#include <wrl/client.h>

#include "base/logging.h"

namespace remoting {

// The appModelId looks arbitrary but it is the same in Win8 and Win10. There is
// no easy way to retrieve the appModelId from the registry.
bool LaunchDefaultAppsSettingsModernDialog() {
  static const wchar_t kControlPanelAppModelId[] =
      L"windows.immersivecontrolpanel_cw5n1h2txyewy"
      L"!microsoft.windows.immersivecontrolpanel";

  Microsoft::WRL::ComPtr<IApplicationActivationManager> activator;
  HRESULT hr = ::CoCreateInstance(CLSID_ApplicationActivationManager, nullptr,
                                  CLSCTX_ALL, IID_PPV_ARGS(&activator));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create IApplicationActivationManager: "
               << logging::SystemErrorCodeToString(hr);
    return false;
  }
  DWORD pid = 0;
  CoAllowSetForegroundWindow(activator.Get(), nullptr);
  hr = activator->ActivateApplication(
      kControlPanelAppModelId, L"page=SettingsPageAppsDefaults", AO_NONE, &pid);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to activate application: "
               << logging::SystemErrorCodeToString(hr);
    return false;
  }
  return true;
}

}  // namespace remoting
