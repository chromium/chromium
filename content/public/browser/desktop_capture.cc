// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/desktop_capture.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/common/content_features.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "content/browser/media/capture/desktop_capturer_lacros.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "content/browser/media/capture/aura_window_to_mojo_device_adapter.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#endif

#if defined(WEBRTC_USE_PIPEWIRE)
#include "base/environment.h"
#include "base/nix/xdg_util.h"
#endif

namespace content::desktop_capture {

webrtc::DesktopCaptureOptions CreateDesktopCaptureOptions() {
  auto options = webrtc::DesktopCaptureOptions::CreateDefault();
  // Leave desktop effects enabled during WebRTC captures.
  options.set_disable_effects(false);
#if BUILDFLAG(IS_WIN)
  static BASE_FEATURE(kDirectXCapturer, "DirectXCapturer",
                      base::FEATURE_ENABLED_BY_DEFAULT);
  if (base::FeatureList::IsEnabled(kDirectXCapturer)) {
    options.set_allow_directx_capturer(true);
    options.set_allow_use_magnification_api(false);
  } else {
    options.set_allow_use_magnification_api(true);
  }
  options.set_enumerate_current_process_windows(
      ShouldEnumerateCurrentProcessWindows());

#elif BUILDFLAG(IS_MAC)
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
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return std::make_unique<DesktopCapturerLacros>(
      DesktopCapturerLacros::CaptureType::kScreen,
      webrtc::DesktopCaptureOptions());
#else
  return webrtc::DesktopCapturer::CreateScreenCapturer(
      CreateDesktopCaptureOptions());
#endif
}

std::unique_ptr<webrtc::DesktopCapturer> CreateWindowCapturer() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return std::make_unique<DesktopCapturerLacros>(
      DesktopCapturerLacros::CaptureType::kWindow,
      webrtc::DesktopCaptureOptions());
#else
  auto options = desktop_capture::CreateDesktopCaptureOptions();
#if defined(RTC_ENABLE_WIN_WGC)
  options.set_allow_wgc_capturer_fallback(true);
#endif
  return webrtc::DesktopCapturer::CreateWindowCapturer(options);
#endif
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void BindAuraWindowCapturer(
    mojo::PendingReceiver<video_capture::mojom::Device> receiver,
    const content::DesktopMediaID& id) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<AuraWindowToMojoDeviceAdapter>(id), std::move(receiver));
}
#endif

bool CanUsePipeWire() {
#if defined(WEBRTC_USE_PIPEWIRE)
  static base::nix::SessionType session_type = base::nix::SessionType::kUnset;
  if (session_type == base::nix::SessionType::kUnset) {
    std::unique_ptr<base::Environment> env = base::Environment::Create();
    session_type = base::nix::GetSessionType(*env);
  }

  return session_type == base::nix::SessionType::kWayland &&
         base::FeatureList::IsEnabled(features::kWebRtcPipeWireCapturer);
#else
  return false;
#endif
}

bool ShouldEnumerateCurrentProcessWindows() {
#if BUILDFLAG(IS_WIN)
  return false;
#else
  return true;
#endif
}

}  // namespace content::desktop_capture
