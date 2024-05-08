// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_APPLE_VIDEO_CAPTURE_DEVICE_FRAME_RECEIVER_H_
#define MEDIA_CAPTURE_VIDEO_APPLE_VIDEO_CAPTURE_DEVICE_FRAME_RECEIVER_H_

#include "base/time/time.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video_capture_types.h"

namespace media {

class CAPTURE_EXPORT VideoCaptureDeviceAVFoundationFrameReceiver {
 public:
  virtual ~VideoCaptureDeviceAVFoundationFrameReceiver() = default;

  // Called to deliver captured video frames.  It's safe to call this method
  // from any thread, including those controlled by AVFoundation.
  virtual void ReceiveFrame(const uint8_t* video_frame,
                            int video_frame_length,
                            const VideoCaptureFormat& frame_format,
                            const gfx::ColorSpace color_space,
                            int aspect_numerator,
                            int aspect_denominator,
                            base::TimeDelta timestamp,
                            std::optional<base::TimeTicks> capture_begin_time,
                            int rotation) = 0;

  // Called to deliver GpuMemoryBuffer-wrapped captured video frames. This
  // function may be called from any thread, including those controlled by
  // AVFoundation.
  virtual void ReceiveExternalGpuMemoryBufferFrame(
      CapturedExternalVideoBuffer frame,
      base::TimeDelta timestamp,
      std::optional<base::TimeTicks> capture_begin_time) = 0;

  // Callbacks with the result of a still image capture, or in case of error,
  // respectively. It's safe to call these methods from any thread.
  virtual void OnPhotoTaken(const uint8_t* image_data,
                            size_t image_length,
                            const std::string& mime_type) = 0;

  // Callback when a call to takePhoto fails.
  virtual void OnPhotoError() = 0;

  // Forwarder to VideoCaptureDevice::Client::OnError().
  virtual void ReceiveError(VideoCaptureError error,
                            const base::Location& from_here,
                            const std::string& reason) = 0;

  // Forwarder to VideoCaptureDevice::Client::OnCaptureConfigurationChanged().
  virtual void ReceiveCaptureConfigurationChanged() = 0;

  // Forwarder to VideoCaptureDevice::Client::OnLog().
  virtual void OnLog(const std::string& message) = 0;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_APPLE_VIDEO_CAPTURE_DEVICE_FRAME_RECEIVER_H_
