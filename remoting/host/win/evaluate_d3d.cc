// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/evaluate_d3d.h"

#include <D3DCommon.h>

#include <iostream>

#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/base/switches.h"
#include "remoting/host/evaluate_capability.h"
#include "remoting/host/win/evaluate_3d_display_mode.h"
#include "third_party/webrtc/modules/desktop_capture/win/dxgi_duplicator_controller.h"
#include "third_party/webrtc/modules/desktop_capture/win/screen_capturer_win_directx.h"

namespace remoting {

namespace {

constexpr char kNoDirectXCapturer[] = "No-DirectX-Capturer";

}  // namespace

int EvaluateD3D() {
  // Creates a capturer instance to avoid the DxgiDuplicatorController to be
  // initialized and deinitialized for each static call to
  // webrtc::ScreenCapturerWinDirectx below.
  webrtc::ScreenCapturerWinDirectx capturer;

  if (webrtc::ScreenCapturerWinDirectx::IsSupported()) {
    // Guaranteed to work.
    // This string is also hard-coded in host_attributes_unittests.cc.
    std::cout << "DirectX-Capturer" << std::endl;
  } else if (webrtc::ScreenCapturerWinDirectx::IsCurrentSessionSupported()) {
    // If we are in a supported session, but DirectX capturer is not able to be
    // initialized. Something must be wrong, we should actively disable it.
    std::cout << kNoDirectXCapturer << std::endl;
  }

  webrtc::DxgiDuplicatorController::D3dInfo info;
  webrtc::ScreenCapturerWinDirectx::RetrieveD3dInfo(&info);
  if (info.min_feature_level < D3D_FEATURE_LEVEL_10_0) {
    std::cout << "MinD3DLT10" << std::endl;
  } else {
    std::cout << "MinD3DGE10" << std::endl;
  }
  if (info.min_feature_level >= D3D_FEATURE_LEVEL_11_0) {
    std::cout << "MinD3DGE11" << std::endl;
  }
  if (info.min_feature_level >= D3D_FEATURE_LEVEL_12_0) {
    std::cout << "MinD3DGE12" << std::endl;
  }

  return kSuccessExitCode;
}

bool BlockD3DCheck() {
  DWORD console_session = WTSGetActiveConsoleSessionId();
  DWORD current_session = 0;
  if (!ProcessIdToSessionId(GetCurrentProcessId(), &current_session)) {
    PLOG(WARNING) << "ProcessIdToSessionId failed: ";
  }

  // Session 0 is not curtained as it does not have an interactive desktop.
  bool is_curtained_session =
      current_session != 0 && current_session != console_session;

  // Skip D3D checks if we are in a curtained session and 3D Display mode is
  // enabled.  Attempting to create a D3D device in this scenario takes a long
  // time which often results in the user being disconnected due to timeouts.
  // After digging in, it looks like the call to D3D11CreateDevice() is spinning
  // while enumerating the display drivers.  There isn't a simple fix for this
  // so instead we should skip the D3D caps check and any other D3D related
  // calls.  This will mean falling back to the GDI capturer.
  return is_curtained_session && Get3dDisplayModeEnabled();
}

bool GetD3DCapabilities(std::vector<std::string>* result) {
  if (BlockD3DCheck()) {
    result->push_back(kNoDirectXCapturer);
    return false;
  }

  std::string d3d_info;
  if (EvaluateCapability(kEvaluateD3D, &d3d_info) != kSuccessExitCode) {
    result->push_back(kNoDirectXCapturer);
    return false;
  }

  auto capabilities =
      base::SplitString(d3d_info, base::kWhitespaceASCII, base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  for (const auto& capability : capabilities) {
    result->push_back(capability);
  }

  return true;
}

bool IsD3DAvailable() {
  if (BlockD3DCheck()) {
    return false;
  }

  std::string unused;
  return (EvaluateCapability(kEvaluateD3D, &unused) == kSuccessExitCode);
}

}  // namespace remoting
