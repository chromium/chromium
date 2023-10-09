// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_STATELESS_QUEUE_H_
#define MEDIA_GPU_V4L2_STATELESS_QUEUE_H_

#include "media/base/video_codecs.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/v4l2/stateless/stateless_device.h"

namespace media {

// V4L2 has two similar queues. Capitalized OUTPUT (for compressed frames)
// and CAPTURE (for uncompressed frames) are the designation that the V4L2
// framework uses. As these are counterintuitive for video decoding this class
// encapsulates the compressed frames into |InputQueue| and uncompressed frames
// into |OutputQueue|.
class MEDIA_GPU_EXPORT BaseQueue {
 public:
  BaseQueue(scoped_refptr<StatelessDevice> device,
            BufferType buffer_type,
            MemoryType memory_type);
  BaseQueue& operator=(const BaseQueue&);
  ~BaseQueue();

  scoped_refptr<StatelessDevice> device_;
  const BufferType buffer_type_;
  const MemoryType memory_type_;
  uint32_t num_planes_;
};

class MEDIA_GPU_EXPORT InputQueue : public BaseQueue {
 public:
  static std::unique_ptr<InputQueue> Create(
      scoped_refptr<StatelessDevice> device,
      const VideoCodec codec,
      const gfx::Size resolution);

  InputQueue(scoped_refptr<StatelessDevice> device, VideoCodec codec);

 private:
  bool SetupFormat(const gfx::Size resolution);

  VideoCodec codec_;
};

}  // namespace media
#endif  // MEDIA_GPU_V4L2_STATELESS_QUEUE_H_
