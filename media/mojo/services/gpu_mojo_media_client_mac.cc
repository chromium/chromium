// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "media/base/audio_decoder.h"
#include "media/base/media_switches.h"
#include "media/base/offloading_audio_encoder.h"
#include "media/filters/mac/audio_toolbox_audio_decoder.h"
#include "media/filters/mac/audio_toolbox_audio_encoder.h"
#include "media/gpu/ipc/service/vda_video_decoder.h"
#include "media/gpu/mac/video_toolbox_video_decoder.h"
#include "media/mojo/services/gpu_mojo_media_client.h"

namespace media {

namespace {

bool UseVTVD() {
  return base::FeatureList::IsEnabled(kVideoToolboxVideoDecoder) &&
         IsMultiPlaneFormatForHardwareVideoEnabled();
}

}  // namespace

std::unique_ptr<VideoDecoder> CreatePlatformVideoDecoder(
    VideoDecoderTraits& traits) {
  if (UseVTVD()) {
    return std::make_unique<VideoToolboxVideoDecoder>(
        traits.task_runner, traits.media_log->Clone(), *traits.gpu_workarounds,
        traits.gpu_task_runner, traits.get_command_buffer_stub_cb);
  }

  return VdaVideoDecoder::Create(
      traits.task_runner, traits.gpu_task_runner, traits.media_log->Clone(),
      *traits.target_color_space, traits.gpu_preferences,
      *traits.gpu_workarounds, traits.get_command_buffer_stub_cb,
      VideoDecodeAccelerator::Config::OutputMode::kAllocate);
}

absl::optional<SupportedVideoDecoderConfigs>
GetPlatformSupportedVideoDecoderConfigs(
    base::WeakPtr<MediaGpuChannelManager> manager,
    gpu::GpuDriverBugWorkarounds gpu_workarounds,
    gpu::GpuPreferences gpu_preferences,
    const gpu::GPUInfo& gpu_info,
    base::OnceCallback<SupportedVideoDecoderConfigs()> get_vda_configs) {
  if (UseVTVD()) {
    return VideoToolboxVideoDecoder::GetSupportedVideoDecoderConfigs(
        gpu_workarounds);
  }
  return std::move(get_vda_configs).Run();
}

std::unique_ptr<AudioDecoder> CreatePlatformAudioDecoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<MediaLog> media_log) {
  return std::make_unique<AudioToolboxAudioDecoder>(std::move(media_log));
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
  if (UseVTVD()) {
    return VideoDecoderType::kVideoToolbox;
  }
  return VideoDecoderType::kVda;
}

}  // namespace media
