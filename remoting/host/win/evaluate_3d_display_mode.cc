// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/evaluate_3d_display_mode.h"

#include <D3DCommon.h>
#include <comdef.h>
#include <dxgi1_3.h>
#include <wrl/client.h>

#include <iostream>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/native_library.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_util.h"
#include "base/win/windows_version.h"
#include "remoting/host/evaluate_capability.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/switches.h"

namespace remoting {

namespace {

constexpr char k3dDisplayModeEnabled[] = "3D-Display-Mode";

typedef HRESULT(WINAPI* CreateDXGIFactory2Function)(UINT Flags,
                                                    REFIID riid,
                                                    void** ppFactory);

}  // namespace

int Evaluate3dDisplayMode() {
  // CreateDXGIFactory2 does not exist prior to Win 8.1 but neither does 3D
  // display mode.
  if (base::win::GetVersion() < base::win::Version::WIN8_1)
    return kSuccessExitCode;

  // We can't directly reference CreateDXGIFactory2 is it does not exist on
  // earlier Windows builds.  Therefore we need a LoadLibrary / GetProcAddress
  // dance.
  base::ScopedNativeLibrary library(base::FilePath(L"dxgi.dll"));
  if (!library.is_valid()) {
    PLOG(INFO) << "Failed to get DXGI library module.";
    return kInitializationFailed;
  }

  CreateDXGIFactory2Function factory_func =
      reinterpret_cast<CreateDXGIFactory2Function>(
          library.GetFunctionPointer("CreateDXGIFactory2"));
  if (!factory_func) {
    PLOG(INFO) << "Failed to get CreateDXGIFactory2 function handle.";
    return kInitializationFailed;
  }

  Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
  HRESULT hr = factory_func(/*flags=*/0, IID_PPV_ARGS(&factory));
  if (hr != S_OK) {
    LOG(WARNING) << "CreateDXGIFactory2 failed: 0x" << std::hex << hr;
    return kInitializationFailed;
  }

  if (factory->IsWindowedStereoEnabled())
    std::cout << k3dDisplayModeEnabled << std::endl;

  return kSuccessExitCode;
}

bool Get3dDisplayModeEnabled() {
  std::string output;
  if (EvaluateCapability(kEvaluate3dDisplayMode, &output) != kSuccessExitCode)
    return false;

  base::TrimString(output, base::kWhitespaceASCII, &output);

  bool is_3d_display_mode_enabled = (output == k3dDisplayModeEnabled);
  LOG_IF(INFO, is_3d_display_mode_enabled) << "3D Display Mode enabled.";

  return is_3d_display_mode_enabled;
}

}  // namespace remoting
