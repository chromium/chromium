// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool.h"
#include "media/base/audio_decoder.h"
#include "media/base/offloading_audio_encoder.h"
#include "media/filters/mac/audio_toolbox_audio_decoder.h"
#include "media/filters/mac/audio_toolbox_audio_encoder.h"
#include "media/gpu/ipc/service/vda_video_decoder.h"
#include "media/mojo/services/gpu_mojo_media_client.h"

namespace media {

std::unique_ptr<VideoDecoder> CreatePlatformVideoDecoder(
    VideoDecoderTraits& traits) {
  return VdaVideoDecoder::Create(
      traits.task_runner, traits.gpu_task_runner, traits.media_log->Clone(),
      *traits.target_color_space, traits.gpu_preferences,
      *traits.gpu_workarounds, traits.get_command_buffer_stub_cb,
      VideoDecodeAccelerator::Config::OutputMode::ALLOCATE);
}

absl::optional<SupportedVideoDecoderConfigs>
GetPlatformSupportedVideoDecoderConfigs(
    gpu::GpuDriverBugWorkarounds gpu_workarounds,
    gpu::GpuPreferences gpu_preferences,
    const gpu::GPUInfo& gpu_info,
    base::OnceCallback<SupportedVideoDecoderConfigs()> get_vda_configs) {
  return std::move(get_vda_configs).Run();
}

std::unique_ptr<AudioDecoder> CreatePlatformAudioDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return std::make_unique<AudioToolboxAudioDecoder>();
}

std::unique_ptr<AudioEncoder> CreatePlatformAudioEncoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  auto encoding_runner = base::ThreadPool::CreateSequencedTaskRunner({});
  auto encoder = std::make_unique<AudioToolboxAudioEncoder>();
  return std::make_unique<OffloadingAudioEncoder>(
      std::move(encoder), std::move(encoding_runner), std::move(task_runner));
}

// This class doesn't exist on mac, so we need a stub for unique_ptr.
class CdmFactory {};

std::unique_ptr<CdmFactory> CreatePlatformCdmFactory(
    mojom::FrameInterfaceFactory* frame_interfaces) {
  return nullptr;
}

VideoDecoderType GetPlatformDecoderImplementationType(
    gpu::GpuDriverBugWorkarounds gpu_workarounds,
    gpu::GpuPreferences gpu_preferences,
    const gpu::GPUInfo& gpu_info) {
  return VideoDecoderType::kVda;
}

}  // namespace media
