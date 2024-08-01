// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_encoder.h"
#include "media/base/media_log.h"
#include "media/base/video_decoder.h"
#include "media/mojo/services/gpu_mojo_media_client.h"

namespace media {

class GpuMojoMediaClientStub final : public GpuMojoMediaClient {
 public:
  GpuMojoMediaClientStub(GpuMojoMediaClientTraits& traits)
      : GpuMojoMediaClient(traits) {}
  ~GpuMojoMediaClientStub() final = default;

 protected:
  std::unique_ptr<VideoDecoder> CreatePlatformVideoDecoder(
      VideoDecoderTraits& traits) final {
    return nullptr;
  }

  std::optional<SupportedVideoDecoderConfigs>
  GetPlatformSupportedVideoDecoderConfigs() final {
    return {};
  }

  VideoDecoderType GetPlatformDecoderImplementationType() final {
    return VideoDecoderType::kUnknown;
  }
};

std::unique_ptr<GpuMojoMediaClient> CreateGpuMediaService(
    GpuMojoMediaClientTraits& traits) {
  return std::make_unique<GpuMojoMediaClientStub>(traits);
}

}  // namespace media
