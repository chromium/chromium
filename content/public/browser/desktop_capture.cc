// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/desktop_capture.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "content/browser/media/capture/pip_screen_capture_coordinator.h"
#include "content/browser/media/capture/screenshot_capture_request_impl.h"
#if BUILDFLAG(IS_MAC)
#include "content/browser/media/capture/desktop_capture_util_mac.h"
#endif
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/features.h"
#include "content/public/common/content_features.h"
#include "third_party/skia/include/core/SkBitmap.h"

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

namespace content::desktop_capture {

webrtc::DesktopCaptureOptions CreateDesktopCaptureOptions() {
  auto options = webrtc::DesktopCaptureOptions::CreateDefault();
  // Leave desktop effects enabled during WebRTC captures.
  options.set_disable_effects(false);
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/webrtc/15045): Possibly remove this flag. Keeping for now
  // to force fallback to GDI.
  static BASE_FEATURE(kDirectXCapturer, base::FEATURE_ENABLED_BY_DEFAULT);
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

namespace {
OpenNativePickerCallbackForTesting& GetCallbackForTesting() {
  static base::NoDestructor<OpenNativePickerCallbackForTesting> callback;
  return *callback;
}
}  // namespace

void OpenNativeScreenCapturePicker(
    content::DesktopMediaID::Type type,
    base::OnceCallback<void(DesktopMediaID::Id)> created_callback,
    base::OnceCallback<void(webrtc::DesktopCapturer::Source)> picker_callback,
    base::OnceCallback<void()> cancel_callback,
    base::OnceCallback<void()> error_callback) {
  if (const auto& testing_callback = GetCallbackForTesting()) {
    testing_callback.Run(type, std::move(created_callback),
                         std::move(picker_callback), std::move(cancel_callback),
                         std::move(error_callback));
    return;
  }
  auto* manager = content::MediaStreamManager::GetInstance();
  if (!manager) {
    if (error_callback) {
      std::move(error_callback).Run();
    }
    return;
  }
  manager->OpenNativeScreenCapturePicker(
      type, std::move(created_callback), std::move(picker_callback),
      std::move(cancel_callback), std::move(error_callback));
}

void CloseNativeScreenCapturePicker(DesktopMediaID source_id) {
  auto* manager = content::MediaStreamManager::GetInstance();
  if (manager && manager->video_capture_manager()) {
    manager->video_capture_manager()->CloseNativeScreenCapturePicker(source_id);
  }
}

#if BUILDFLAG(IS_MAC)
void GetApplicationAudioCaptureId(
    DesktopMediaID desktop_media_id,
    GetApplicationAudioCaptureIdCallback callback) {
  content::GetApplicationAudioCaptureIdInternal(desktop_media_id,
                                                std::move(callback));
}
#endif  // BUILDFLAG(IS_MAC)

void SetOpenNativeScreenCapturePickerCallbackForTesting(  // IN-TEST
    OpenNativePickerCallbackForTesting callback) {
  GetCallbackForTesting() = std::move(callback);  // IN-TEST
}

std::unique_ptr<ScreenshotCaptureRequest> CaptureScreenshot(
    DesktopMediaID source,
    base::OnceCallback<void(const ::SkBitmap&)> callback) {
  return CreateScreenshotCaptureRequest(source, std::move(callback));
}

}  // namespace content::desktop_capture
