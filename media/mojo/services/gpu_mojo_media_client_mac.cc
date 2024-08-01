// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "media/base/audio_decoder.h"
#include "media/base/offloading_audio_encoder.h"
#include "media/filters/mac/audio_toolbox_audio_decoder.h"
#include "media/filters/mac/audio_toolbox_audio_encoder.h"
#include "media/gpu/mac/video_toolbox_video_decoder.h"
#include "media/mojo/services/gpu_mojo_media_client.h"

namespace media {

class GpuMojoMediaClientMac final : public GpuMojoMediaClient {
 public:
  GpuMojoMediaClientMac(GpuMojoMediaClientTraits& traits)
      : GpuMojoMediaClient(traits) {}
  ~GpuMojoMediaClientMac() final = default;

 protected:
  std::unique_ptr<VideoDecoder> CreatePlatformVideoDecoder(
      VideoDecoderTraits& traits) final {
    return std::make_unique<VideoToolboxVideoDecoder>(
        traits.task_runner, traits.media_log->Clone(), gpu_workarounds_,
        gpu_task_runner_, traits.get_command_buffer_stub_cb);
  }

  std::optional<SupportedAudioDecoderConfigs>
  GetPlatformSupportedAudioDecoderConfigs() final {
    SupportedAudioDecoderConfigs audio_configs;
    audio_configs.emplace_back(AudioCodec::kAAC, AudioCodecProfile::kXHE_AAC);
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
    audio_configs.emplace_back(AudioCodec::kAC3, AudioCodecProfile::kUnknown);
    audio_configs.emplace_back(AudioCodec::kEAC3, AudioCodecProfile::kUnknown);
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
    return audio_configs;
  }

  std::optional<SupportedVideoDecoderConfigs>
  GetPlatformSupportedVideoDecoderConfigs() final {
    return VideoToolboxVideoDecoder::GetSupportedVideoDecoderConfigs(
        gpu_workarounds_);
  }

  std::unique_ptr<AudioDecoder> CreatePlatformAudioDecoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::unique_ptr<MediaLog> media_log) final {
    return std::make_unique<AudioToolboxAudioDecoder>(std::move(media_log));
  }

  std::unique_ptr<AudioEncoder> CreatePlatformAudioEncoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner) final {
    auto encoding_runner = base::ThreadPool::CreateSequencedTaskRunner({});
    auto encoder = std::make_unique<AudioToolboxAudioEncoder>();
    return std::make_unique<OffloadingAudioEncoder>(
        std::move(encoder), std::move(encoding_runner), std::move(task_runner));
  }

  VideoDecoderType GetPlatformDecoderImplementationType() final {
    return VideoDecoderType::kVideoToolbox;
  }
};

std::unique_ptr<GpuMojoMediaClient> CreateGpuMediaService(
    GpuMojoMediaClientTraits& traits) {
  return std::make_unique<GpuMojoMediaClientMac>(traits);
}

}  // namespace media
