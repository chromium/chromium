// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/desktop_capture.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/features.h"
#include "content/public/common/content_features.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "content/browser/media/capture/desktop_capturer_ash.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/media/capture/desktop_capturer_android.h"
#endif

#if defined(WEBRTC_USE_PIPEWIRE)
#include "base/environment.h"
#include "base/nix/xdg_util.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"

// CGDisplayStreamCreate() is marked as deprecated from macOS 14 (Sonoma), so
// don't use unless the feature flag is set.
bool CGDisplayStreamCreateIsAvailable() {
  if (base::mac::MacOSMajorVersion() >= 14) {
    return false;
  }
  return true;
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
// Enabled-by-default, but exists as a kill-switch.
// TODO(crbug.com/409473386): Remove this flag once it has been in stable for a
// few milestones.
BASE_FEATURE(kUseHeuristicForWindowsFullScreenPowerPoint,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls the rollout of a finch experiment.
// TODO(crbug.com/409473386): Remove this feature once it has been rolled out to
// stable for a few milestones.
BASE_FEATURE(kUseFullScreenHeuristicForWgc, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

namespace content::desktop_capture {

webrtc::DesktopCaptureOptions CreateDesktopCaptureOptions() {
  auto options = webrtc::DesktopCaptureOptions::CreateDefault();
  // Leave desktop effects enabled during WebRTC captures.
  options.set_disable_effects(false);
#if BUILDFLAG(IS_WIN)
  options.full_screen_window_detector()
      ->SetUseHeuristicFullscreenPowerPointWindows(
          base::FeatureList::IsEnabled(
              kUseHeuristicForWindowsFullScreenPowerPoint),
          base::FeatureList::IsEnabled(kUseFullScreenHeuristicForWgc));

  // TODO(crbug.com/webrtc/15045): Possibly remove this flag. Keeping for now
  // to force fallback to GDI.
  static BASE_FEATURE(kDirectXCapturer, "DirectXCapturer",
                      base::FEATURE_ENABLED_BY_DEFAULT);
  if (base::FeatureList::IsEnabled(kDirectXCapturer)) {
    // Results in DirectX as main capture API and GDI as fallback solution.
    options.set_allow_directx_capturer(true);
  }
  options.set_enumerate_current_process_windows(
      ShouldEnumerateCurrentProcessWindows());

#elif BUILDFLAG(IS_MAC)
  // Enabling IO surface capturer means that we will be using the
  // CGDisplayStreamCreate() API. This is marked as deprecated from macOS 14
  // (Sonoma), only use it if it's available.
  if (base::FeatureList::IsEnabled(features::kIOSurfaceCapturer) &&
      CGDisplayStreamCreateIsAvailable()) {
    options.set_allow_iosurface(true);
  }
#endif
#if defined(WEBRTC_USE_PIPEWIRE)
  options.set_allow_pipewire(true);
#endif  // defined(WEBRTC_USE_PIPEWIRE)
  return options;
}

std::unique_ptr<webrtc::DesktopCapturer> CreateScreenCapturer(
    webrtc::DesktopCaptureOptions options,
    bool for_snapshot) {
#if BUILDFLAG(IS_CHROMEOS)
  if (for_snapshot) {
    return std::make_unique<DesktopCapturerAsh>();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<DesktopCapturerAndroid>(options);
#else
  return webrtc::DesktopCapturer::CreateScreenCapturer(options);
#endif  // BUILDFLAG(IS_ANDROID)
}

std::unique_ptr<webrtc::DesktopCapturer> CreateWindowCapturer(
    webrtc::DesktopCaptureOptions options) {
#if defined(RTC_ENABLE_WIN_WGC)
  options.set_allow_wgc_capturer_fallback(true);
#endif  // defined(RTC_ENABLE_WIN_WGC)

#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<DesktopCapturerAndroid>(options);
#else
  return webrtc::DesktopCapturer::CreateWindowCapturer(options);
#endif  // BUILDFLAG(IS_ANDROID)
}

bool CanUsePipeWire() {
#if defined(WEBRTC_USE_PIPEWIRE)
  static base::nix::SessionType session_type = base::nix::SessionType::kUnset;
  if (session_type == base::nix::SessionType::kUnset) {
    std::unique_ptr<base::Environment> env = base::Environment::Create();
    session_type = base::nix::GetSessionType(*env);
  }

  return session_type == base::nix::SessionType::kWayland;
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

void OpenNativeScreenCapturePicker(
    content::DesktopMediaID::Type type,
    base::OnceCallback<void(DesktopMediaID::Id)> created_callback,
    base::OnceCallback<void(webrtc::DesktopCapturer::Source)> picker_callback,
    base::OnceCallback<void()> cancel_callback,
    base::OnceCallback<void()> error_callback) {
  content::MediaStreamManager::GetInstance()
      ->video_capture_manager()
      ->OpenNativeScreenCapturePicker(
          type, std::move(created_callback), std::move(picker_callback),
          std::move(cancel_callback), std::move(error_callback));
}

void CloseNativeScreenCapturePicker(DesktopMediaID source_id) {
  content::MediaStreamManager::GetInstance()
      ->video_capture_manager()
      ->CloseNativeScreenCapturePicker(source_id);
}

}  // namespace content::desktop_capture
