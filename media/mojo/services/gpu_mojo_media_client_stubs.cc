// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_decoder.h"
#include "media/base/audio_encoder.h"
#include "media/base/video_decoder.h"
#include "media/mojo/services/gpu_mojo_media_client.h"

namespace media {

std::unique_ptr<VideoDecoder> CreatePlatformVideoDecoder(
    VideoDecoderTraits& traits) {
  return nullptr;
}

absl::optional<SupportedVideoDecoderConfigs>
GetPlatformSupportedVideoDecoderConfigs(
    gpu::GpuDriverBugWorkarounds gpu_workarounds,
    gpu::GpuPreferences gpu_preferences,
    const gpu::GPUInfo& gpu_info,
    base::OnceCallback<SupportedVideoDecoderConfigs()> get_vda_configs) {
  return {};
}

std::unique_ptr<AudioDecoder> CreatePlatformAudioDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return nullptr;
}

std::unique_ptr<AudioEncoder> CreatePlatformAudioEncoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  return nullptr;
}

// This class doesn't exist on any of the platforms that use the stubs.
class CdmFactory {};

std::unique_ptr<CdmFactory> CreatePlatformCdmFactory(
    mojom::FrameInterfaceFactory* frame_interfaces) {
  return nullptr;
}

VideoDecoderType GetPlatformDecoderImplementationType(
    gpu::GpuDriverBugWorkarounds gpu_workarounds,
    gpu::GpuPreferences gpu_preferences,
    const gpu::GPUInfo& gpu_info) {
  return VideoDecoderType::kUnknown;
}

}  // namespace media
