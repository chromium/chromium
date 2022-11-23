// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_PROVIDER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_PROVIDER_H_

#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_capture_device_info.h"
#include "media/capture/video/video_frame_receiver.h"
#include "media/capture/video_capture_types.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

namespace content {

class VideoCaptureDeviceLauncher;

// Note: GetDeviceInfosAsync is only relevant for devices with
// MediaStreamType == DEVICE_VIDEO_CAPTURE, i.e. camera devices.
class VideoCaptureProvider {
 public:
  using GetDeviceInfosCallback = base::OnceCallback<void(
      media::mojom::DeviceEnumerationResult result_code,
      const std::vector<media::VideoCaptureDeviceInfo>&)>;

  virtual ~VideoCaptureProvider() {}

  // The passed-in |result_callback| must guarantee that the called
  // instance stays alive until |result_callback| is invoked.
  virtual void GetDeviceInfosAsync(GetDeviceInfosCallback result_callback) = 0;

  virtual std::unique_ptr<VideoCaptureDeviceLauncher>
  CreateDeviceLauncher() = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_PROVIDER_H_
