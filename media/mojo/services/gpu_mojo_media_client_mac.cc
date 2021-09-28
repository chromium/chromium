// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_decoder.h"
#include "media/gpu/ipc/service/vda_video_decoder.h"
#include "media/mojo/services/gpu_mojo_media_client.h"

namespace media {
namespace {

class MacPlatformDelegate : public GpuMojoMediaClient::PlatformDelegate {
 public:
  explicit MacPlatformDelegate(GpuMojoMediaClient* client) : client_(client) {}
  ~MacPlatformDelegate() override = default;

  MacPlatformDelegate(const MacPlatformDelegate&) = delete;
  void operator=(const MacPlatformDelegate&) = delete;

  // GpuMojoMediaClient::PlatformDelegate implementation.
  std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      const VideoDecoderTraits& traits) override {
    return VdaVideoDecoder::Create(
        traits.task_runner, traits.gpu_task_runner, traits.media_log->Clone(),
        *traits.target_color_space, client_->gpu_preferences(),
        client_->gpu_workarounds(), traits.get_command_buffer_stub_cb);
  }

  void GetSupportedVideoDecoderConfigs(
      MojoMediaClient::SupportedVideoDecoderConfigsCallback callback) override {
    std::move(callback).Run(client_->GetVDAVideoDecoderConfigs());
  }

  VideoDecoderType GetDecoderImplementationType() override {
    return VideoDecoderType::kVda;
  }

 private:
  GpuMojoMediaClient* client_;
};

}  // namespace

std::unique_ptr<GpuMojoMediaClient::PlatformDelegate>
GpuMojoMediaClient::PlatformDelegate::Create(GpuMojoMediaClient* client) {
  return std::make_unique<MacPlatformDelegate>(client);
}

}  // namespace media
