// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/desktop_capture.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/common/content_features.h"

#if BUILDFLAG(IS_LACROS)
#include "content/browser/media/capture/desktop_capturer_lacros.h"
#endif

namespace content {
namespace desktop_capture {

webrtc::DesktopCaptureOptions CreateDesktopCaptureOptions() {
  auto options = webrtc::DesktopCaptureOptions::CreateDefault();
  // Leave desktop effects enabled during WebRTC captures.
  options.set_disable_effects(false);
#if defined(OS_WIN)
  static constexpr base::Feature kDirectXCapturer{
      "DirectXCapturer",
      base::FEATURE_ENABLED_BY_DEFAULT};
  if (base::FeatureList::IsEnabled(kDirectXCapturer)) {
    options.set_allow_directx_capturer(true);
    options.set_allow_use_magnification_api(false);
  } else {
    options.set_allow_use_magnification_api(true);
  }
#elif defined(OS_MAC)
  if (base::FeatureList::IsEnabled(features::kIOSurfaceCapturer)) {
    options.set_allow_iosurface(true);
  }
#endif
#if defined(WEBRTC_USE_PIPEWIRE)
  if (base::FeatureList::IsEnabled(features::kWebRtcPipeWireCapturer)) {
    options.set_allow_pipewire(true);
  }
#endif  // defined(WEBRTC_USE_PIPEWIRE)
  return options;
}

std::unique_ptr<webrtc::DesktopCapturer> CreateScreenCapturer() {
#if BUILDFLAG(IS_LACROS)
  return std::make_unique<DesktopCapturerLacros>(
      DesktopCapturerLacros::CaptureType::kScreen,
      webrtc::DesktopCaptureOptions());
#else
  return webrtc::DesktopCapturer::CreateScreenCapturer(
      CreateDesktopCaptureOptions());
#endif
}

std::unique_ptr<webrtc::DesktopCapturer> CreateWindowCapturer() {
#if BUILDFLAG(IS_LACROS)
  return std::make_unique<DesktopCapturerLacros>(
      DesktopCapturerLacros::CaptureType::kWindow,
      webrtc::DesktopCaptureOptions());
#else
  return webrtc::DesktopCapturer::CreateWindowCapturer(
      CreateDesktopCaptureOptions());
#endif
}

}  // namespace desktop_capture
}  // namespace content
