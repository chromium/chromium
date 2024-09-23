// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/desktop_capture.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/features.h"
#include "content/public/common/content_features.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "content/browser/media/capture/desktop_capturer_lacros.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "content/browser/media/capture/aura_window_to_mojo_device_adapter.h"
#include "content/browser/media/capture/desktop_capturer_ash.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
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

BASE_FEATURE(kUseCGDisplayStreamCreateSonoma,
             "UseCGDisplayStreamCreateSonoma",
             base::FEATURE_DISABLED_BY_DEFAULT);

// CGDisplayStreamCreate() is marked as deprecated from macOS 14 (Sonoma), so
// don't use unless the feature flag is set.
bool CGDisplayStreamCreateIsAvailable() {
  if (base::mac::MacOSMajorVersion() >= 14) {
    return base::FeatureList::IsEnabled(kUseCGDisplayStreamCreateSonoma);
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
  if (base::FeatureList::IsEnabled(features::kWebRtcPipeWireCapturer)) {
    options.set_allow_pipewire(true);
  }
#endif  // defined(WEBRTC_USE_PIPEWIRE)
  return options;
}

std::unique_ptr<webrtc::DesktopCapturer> CreateScreenCapturer(
    bool allow_wgc_screen_capturer) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return std::make_unique<DesktopCapturerLacros>(
      DesktopCapturerLacros::CaptureType::kScreen,
      webrtc::DesktopCaptureOptions());
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  return std::make_unique<DesktopCapturerAsh>();
#else
  auto options = desktop_capture::CreateDesktopCaptureOptions();
#if defined(RTC_ENABLE_WIN_WGC)
  if (allow_wgc_screen_capturer) {
    options.set_allow_wgc_screen_capturer(true);
  }
#endif  // defined(RTC_ENABLE_WIN_WGC)
  return webrtc::DesktopCapturer::CreateScreenCapturer(options);
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
