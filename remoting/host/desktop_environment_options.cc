// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_environment_options.h"

#include <string>
#include <utility>

#include "base/optional.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "remoting/host/win/evaluate_d3d.h"
#endif

namespace remoting {

using DesktopCaptureOptions = webrtc::DesktopCaptureOptions;

// static
DesktopEnvironmentOptions DesktopEnvironmentOptions::CreateDefault() {
  DesktopEnvironmentOptions options;
  options.desktop_capture_options_ = DesktopCaptureOptions::CreateDefault();
  options.Initialize();
  return options;
}

DesktopEnvironmentOptions::DesktopEnvironmentOptions() {
  Initialize();
}

DesktopEnvironmentOptions::DesktopEnvironmentOptions(
    DesktopEnvironmentOptions&& other) = default;
DesktopEnvironmentOptions::DesktopEnvironmentOptions(
    const DesktopEnvironmentOptions& other) = default;
DesktopEnvironmentOptions::~DesktopEnvironmentOptions() = default;
DesktopEnvironmentOptions&
DesktopEnvironmentOptions::operator=(
    DesktopEnvironmentOptions&& other) = default;
DesktopEnvironmentOptions&
DesktopEnvironmentOptions::operator=(
    const DesktopEnvironmentOptions& other) = default;

void DesktopEnvironmentOptions::Initialize() {
  desktop_capture_options_.set_detect_updated_region(true);
#if defined (OS_WIN)
  // Whether DirectX capturer can be enabled depends on various factors,
  // including how many applications are using related APIs.
  // WebRTC/DesktopCapturer will take care of those details. This check is used
  // to ensure D3D APIs are safe to access in the current environment.
  desktop_capture_options_.set_allow_directx_capturer(IsD3DAvailable());
#endif
}

const DesktopCaptureOptions*
DesktopEnvironmentOptions::desktop_capture_options() const {
  return &desktop_capture_options_;
}

DesktopCaptureOptions*
DesktopEnvironmentOptions::desktop_capture_options() {
  return &desktop_capture_options_;
}

bool DesktopEnvironmentOptions::enable_curtaining() const {
  return enable_curtaining_;
}

void DesktopEnvironmentOptions::set_enable_curtaining(bool enabled) {
  enable_curtaining_ = enabled;
}

bool DesktopEnvironmentOptions::enable_user_interface() const {
  return enable_user_interface_;
}

void DesktopEnvironmentOptions::set_enable_user_interface(bool enabled) {
  enable_user_interface_ = enabled;
}

bool DesktopEnvironmentOptions::terminate_upon_input() const {
  return terminate_upon_input_;
}

void DesktopEnvironmentOptions::set_terminate_upon_input(bool enabled) {
  terminate_upon_input_ = enabled;
}

bool DesktopEnvironmentOptions::enable_file_transfer() const {
  return enable_file_transfer_;
}

void DesktopEnvironmentOptions::set_enable_file_transfer(bool enabled) {
  enable_file_transfer_ = enabled;
}

void DesktopEnvironmentOptions::ApplySessionOptions(
    const SessionOptions& options) {
#if defined(OS_WIN)
  base::Optional<bool> directx_capturer =
      options.GetBool("DirectX-Capturer");
  if (directx_capturer) {
    desktop_capture_options_.set_allow_directx_capturer(*directx_capturer);
  }
#endif
  // This field is for test purpose. Usually it should not be set to false.
  base::Optional<bool> detect_updated_region =
      options.GetBool("Detect-Updated-Region");
  if (detect_updated_region) {
    desktop_capture_options_.set_detect_updated_region(*detect_updated_region);
  }
}

}  // namespace remoting
