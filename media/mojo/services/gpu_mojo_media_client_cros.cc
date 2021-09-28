// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/gpu_mojo_media_client.h"

#include "media/base/audio_decoder.h"
#include "media/base/cdm_factory.h"
#include "media/gpu/chromeos/mailbox_video_frame_converter.h"
#include "media/gpu/chromeos/platform_video_frame_pool.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/gpu/ipc/service/vda_video_decoder.h"

#if defined(OS_CHROMEOS)
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_factory.h"
#endif  // defined(OS_CHROMEOS)

namespace media {
namespace {

bool ShouldUseChromeOSDirectVideoDecoder(
    const gpu::GpuPreferences& gpu_preferences) {
#if defined(OS_CHROMEOS)
  return gpu_preferences.enable_chromeos_direct_video_decoder;
#else
  return false;
#endif
}

class CrosPlatformDelegate : public GpuMojoMediaClient::PlatformDelegate {
 public:
  explicit CrosPlatformDelegate(GpuMojoMediaClient* client) : client_(client) {}
  ~CrosPlatformDelegate() override = default;

  CrosPlatformDelegate(const CrosPlatformDelegate&) = delete;
  void operator=(const CrosPlatformDelegate&) = delete;

  // GpuMojoMediaClient::PlatformDelegate implementation.
  std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      const VideoDecoderTraits& traits) override {
    if (ShouldUseChromeOSDirectVideoDecoder(client_->gpu_preferences())) {
      auto frame_pool = std::make_unique<PlatformVideoFramePool>(
          traits.gpu_memory_buffer_factory);
      auto frame_converter = MailboxVideoFrameConverter::Create(
          base::BindRepeating(&PlatformVideoFramePool::UnwrapFrame,
                              base::Unretained(frame_pool.get())),
          traits.gpu_task_runner, traits.get_command_buffer_stub_cb);
      return VideoDecoderPipeline::Create(
          traits.task_runner, std::move(frame_pool), std::move(frame_converter),
          traits.media_log->Clone());
    }
    return VdaVideoDecoder::Create(
        traits.task_runner, traits.gpu_task_runner, traits.media_log->Clone(),
        *traits.target_color_space, client_->gpu_preferences(),
        client_->gpu_workarounds(), traits.get_command_buffer_stub_cb);
  }

  void GetSupportedVideoDecoderConfigs(
      MojoMediaClient::SupportedVideoDecoderConfigsCallback callback) override {
    SupportedVideoDecoderConfigs supported_configs;
    if (ShouldUseChromeOSDirectVideoDecoder(client_->gpu_preferences())) {
      std::move(callback).Run(*VideoDecoderPipeline::GetSupportedConfigs(
          client_->gpu_workarounds()));
      return;
    }
    std::move(callback).Run(client_->GetVDAVideoDecoderConfigs());
  }

  std::unique_ptr<CdmFactory> CreateCdmFactory(
      mojom::FrameInterfaceFactory* frame_interfaces) override {
#if defined(OS_CHROMEOS)
    return std::make_unique<chromeos::ChromeOsCdmFactory>(frame_interfaces);
#else   // defined(OS_CHROMEOS)
    return nullptr;
#endif  // else defined(OS_CHROMEOS)
  }

  VideoDecoderType GetDecoderImplementationType() override {
    if (ShouldUseChromeOSDirectVideoDecoder(client_->gpu_preferences())) {
      return VideoDecoderType::kVaapi;
    }
    return VideoDecoderType::kVda;
  }

 private:
  GpuMojoMediaClient* client_;
};

}  // namespace

std::unique_ptr<GpuMojoMediaClient::PlatformDelegate>
GpuMojoMediaClient::PlatformDelegate::Create(GpuMojoMediaClient* client) {
  return std::make_unique<CrosPlatformDelegate>(client);
}

}  // namespace media
