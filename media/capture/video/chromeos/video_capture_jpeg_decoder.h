// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_JPEG_DECODER_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_JPEG_DECODER_H_

#include "base/functional/callback.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_frame_receiver.h"

namespace media {

// All methods are allowed to be called from any thread, but calls must be
// non-concurrently.
class CAPTURE_EXPORT VideoCaptureJpegDecoder {
 public:
  // Enumeration of decoder status. The enumeration is published for clients to
  // decide the behavior according to STATUS.
  enum STATUS {
    INIT_PENDING,  // Default value while waiting initialization finished.
    INIT_PASSED,   // Initialization succeed.
    FAILED,        // JPEG decode is not supported, initialization failed, or
                   // decode error.
  };

  using DecodeDoneCB = base::RepeatingCallback<void(ReadyFrameInBuffer)>;

  virtual ~VideoCaptureJpegDecoder() {}

  // Creates and initializes decoder asynchronously.
  virtual void Initialize() = 0;

  // Returns initialization status.
  virtual STATUS GetStatus() const = 0;

  // Decodes a JPEG picture.
  virtual void DecodeCapturedData(
      const uint8_t* data,
      size_t in_buffer_size,
      const VideoCaptureFormat& frame_format,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp,
      VideoCaptureDevice::Client::Buffer out_buffer) = 0;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_JPEG_DECODER_H_
