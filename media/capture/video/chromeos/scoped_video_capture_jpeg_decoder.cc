// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/scoped_video_capture_jpeg_decoder.h"

namespace media {

ScopedVideoCaptureJpegDecoder::ScopedVideoCaptureJpegDecoder(
    std::unique_ptr<VideoCaptureJpegDecoder> decoder,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : decoder_(std::move(decoder)), task_runner_(std::move(task_runner)) {}

ScopedVideoCaptureJpegDecoder::~ScopedVideoCaptureJpegDecoder() {
  task_runner_->DeleteSoon(FROM_HERE, std::move(decoder_));
}

// Implementation of VideoCaptureJpegDecoder:
void ScopedVideoCaptureJpegDecoder::Initialize() {
  decoder_->Initialize();
}

VideoCaptureJpegDecoder::STATUS ScopedVideoCaptureJpegDecoder::GetStatus()
    const {
  return decoder_->GetStatus();
}

void ScopedVideoCaptureJpegDecoder::DecodeCapturedData(
    const uint8_t* data,
    size_t in_buffer_size,
    const media::VideoCaptureFormat& frame_format,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    media::VideoCaptureDevice::Client::Buffer out_buffer) {
  decoder_->DecodeCapturedData(data, in_buffer_size, frame_format,
                               reference_time, timestamp,
                               std::move(out_buffer));
}

}  // namespace media
