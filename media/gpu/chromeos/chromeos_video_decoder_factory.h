// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_CHROMEOS_VIDEO_DECODER_FACTORY_H_
#define MEDIA_GPU_CHROMEOS_CHROMEOS_VIDEO_DECODER_FACTORY_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "media/base/media_log.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/gpu/media_gpu_export.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace gpu {
class GpuDriverBugWorkarounds;
}

namespace media {

class DmabufVideoFramePool;
class VideoDecoder;
class VideoFrameConverter;

class MEDIA_GPU_EXPORT ChromeosVideoDecoderFactory {
 public:
  static SupportedVideoDecoderConfigs GetSupportedConfigs(
      const gpu::GpuDriverBugWorkarounds& workarounds);

  // Create VideoDecoder instance that allocates VideoFrame from |frame_pool|
  // and converts the output VideoFrame |frame_converter|.
  static std::unique_ptr<VideoDecoder> Create(
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      std::unique_ptr<DmabufVideoFramePool> frame_pool,
      std::unique_ptr<VideoFrameConverter> frame_converter,
      std::unique_ptr<MediaLog> media_log);
};

}  // namespace media
#endif  // MEDIA_GPU_CHROMEOS_CHROMEOS_VIDEO_DECODER_FACTORY_H_
