// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/gpu_mojo_media_client.h"

#include "media/base/audio_decoder.h"
#include "media/gpu/chromeos/mailbox_video_frame_converter.h"
#include "media/gpu/chromeos/platform_video_frame_pool.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/gpu/ipc/service/vda_video_decoder.h"

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

}  // namespace

std::unique_ptr<VideoDecoder> CreatePlatformVideoDecoder(
    const VideoDecoderTraits& traits) {
  if (ShouldUseChromeOSDirectVideoDecoder(traits.gpu_preferences)) {
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
      *traits.target_color_space, traits.gpu_preferences,
      *traits.gpu_workarounds, traits.get_command_buffer_stub_cb);
}

absl::optional<SupportedVideoDecoderConfigs>
GetPlatformSupportedVideoDecoderConfigs(
    gpu::GpuDriverBugWorkarounds gpu_workarounds,
    gpu::GpuPreferences gpu_preferences,
    base::OnceCallback<SupportedVideoDecoderConfigs()> get_vda_configs) {
  SupportedVideoDecoderConfigs supported_configs;
  if (ShouldUseChromeOSDirectVideoDecoder(gpu_preferences)) {
    return VideoDecoderPipeline::GetSupportedConfigs(gpu_workarounds);
  }
  return std::move(get_vda_configs).Run();
}

VideoDecoderType GetPlatformDecoderImplementationType(
    gpu::GpuDriverBugWorkarounds gpu_workarounds,
    gpu::GpuPreferences gpu_preferences) {
  if (ShouldUseChromeOSDirectVideoDecoder(gpu_preferences)) {
    return VideoDecoderType::kVaapi;
  }
  return VideoDecoderType::kVda;
}

std::unique_ptr<AudioDecoder> CreatePlatformAudioDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return nullptr;
}

// When |IS_CHROMEOS_ASH|, gpu_mojo_media_client_cros_ash.cc is built, which
// has the real implementation of this method.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
class CdmFactory {};

std::unique_ptr<CdmFactory> CreatePlatformCdmFactory(
    mojom::FrameInterfaceFactory* frame_interfaces) {
  return nullptr;
}
#endif

}  // namespace media
