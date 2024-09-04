// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_PROVIDER_SWITCHER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_PROVIDER_SWITCHER_H_

#include "content/browser/renderer_host/media/video_capture_provider.h"
#include "content/common/content_export.h"

namespace content {

// Routes requests for media devices, e.g. cameras, to
// |media_device_capture_provider| and for all other types of capture, e.g.
// screen or tab capture, to the given |other_types_capture_provider|.
class CONTENT_EXPORT VideoCaptureProviderSwitcher
    : public VideoCaptureProvider {
 public:
  VideoCaptureProviderSwitcher(
      std::unique_ptr<VideoCaptureProvider> media_device_capture_provider,
      std::unique_ptr<VideoCaptureProvider> other_types_capture_provider);
  ~VideoCaptureProviderSwitcher() override;

  void GetDeviceInfosAsync(GetDeviceInfosCallback result_callback) override;

  std::unique_ptr<VideoCaptureDeviceLauncher> CreateDeviceLauncher() override;

  void OpenNativeScreenCapturePicker(
      content::DesktopMediaID::Type type,
      base::OnceCallback<void(DesktopMediaID::Id)> created_callback,
      base::OnceCallback<void(webrtc::DesktopCapturer::Source)> picker_callback,
      base::OnceCallback<void()> cancel_callback,
      base::OnceCallback<void()> error_callback) override;

  void CloseNativeScreenCapturePicker(DesktopMediaID device_id) override;

 private:
  const std::unique_ptr<VideoCaptureProvider> media_device_capture_provider_;
  const std::unique_ptr<VideoCaptureProvider> other_types_capture_provider_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_PROVIDER_SWITCHER_H_
