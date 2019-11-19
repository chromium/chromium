// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_SCOPED_VIDEO_CAPTURE_JPEG_DECODER_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_SCOPED_VIDEO_CAPTURE_JPEG_DECODER_H_

#include <memory>

#include "base/sequenced_task_runner.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/chromeos/video_capture_jpeg_decoder.h"

namespace media {

// Decorator for media::VideoCaptureJpegDecoder that destroys the decorated
// instance on a given task runner.
class CAPTURE_EXPORT ScopedVideoCaptureJpegDecoder
    : public VideoCaptureJpegDecoder {
 public:
  ScopedVideoCaptureJpegDecoder(
      std::unique_ptr<VideoCaptureJpegDecoder> decoder,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  ~ScopedVideoCaptureJpegDecoder() override;

  // Implementation of VideoCaptureJpegDecoder:
  void Initialize() override;
  STATUS GetStatus() const override;
  void DecodeCapturedData(
      const uint8_t* data,
      size_t in_buffer_size,
      const media::VideoCaptureFormat& frame_format,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp,
      media::VideoCaptureDevice::Client::Buffer out_buffer) override;

 private:
  std::unique_ptr<VideoCaptureJpegDecoder> decoder_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_SCOPED_VIDEO_CAPTURE_JPEG_DECODER_H_
