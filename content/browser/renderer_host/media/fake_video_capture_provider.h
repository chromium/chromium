// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_FAKE_VIDEO_CAPTURE_PROVIDER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_FAKE_VIDEO_CAPTURE_PROVIDER_H_

#include "content/browser/renderer_host/media/video_capture_provider.h"
#include "media/capture/video/video_capture_system_impl.h"

namespace content {

// Implementation of VideoCaptureProvider that produces fake devices
// generating test frames.
class FakeVideoCaptureProvider : public VideoCaptureProvider {
 public:
  FakeVideoCaptureProvider();
  ~FakeVideoCaptureProvider() override;

  // VideoCaptureProvider implementation.
  void GetDeviceInfosAsync(GetDeviceInfosCallback result_callback) override;
  std::unique_ptr<VideoCaptureDeviceLauncher> CreateDeviceLauncher() override;

 private:
  media::VideoCaptureSystemImpl system_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_FAKE_VIDEO_CAPTURE_PROVIDER_H_
