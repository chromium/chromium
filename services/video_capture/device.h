// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_DEVICE_H_
#define SERVICES_VIDEO_CAPTURE_DEVICE_H_

#include "media/capture/video/video_capture_device_client.h"
#include "services/video_capture/public/mojom/device.mojom.h"

namespace media {
class VideoFrameReceiver;
}

namespace video_capture {

class Device : public mojom::Device {
 public:
  virtual void StartInProcess(
      const media::VideoCaptureParams& requested_settings,
      const base::WeakPtr<media::VideoFrameReceiver>& frame_handler,
      media::VideoEffectsContext context) {}
  virtual void StopInProcess() {}
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_DEVICE_H_
