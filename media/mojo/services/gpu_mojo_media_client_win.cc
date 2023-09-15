// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/gpu_mojo_media_client.h"

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "media/base/audio_decoder.h"
#include "media/base/media_switches.h"
#include "media/base/offloading_audio_encoder.h"
#include "media/filters/win/media_foundation_audio_decoder.h"
#include "media/gpu/ipc/service/media_gpu_channel_manager.h"
#include "media/gpu/windows/d3d11_video_decoder.h"
#include "media/gpu/windows/mf_audio_encoder.h"
#include "ui/gl/direct_composition_support.h"

namespace media {

namespace {

D3D11VideoDecoder::GetD3D11DeviceCB GetD3D11DeviceCallback(
    ComD3D11Device d3d11_device) {
  return base::BindRepeating(
      [](ComD3D11Device d3d11_device) { return d3d11_device; },
      std::move(d3d11_device));
}

}  // namespace

std::unique_ptr<VideoDecoder> CreatePlatformVideoDecoder(
    VideoDecoderTraits& traits) {
  if (traits.gpu_workarounds->disable_d3d11_video_decoder) {
    return nullptr;
  }
  // Report that HDR is enabled if any display has HDR enabled.
  bool hdr_enabled = false;
  auto dxgi_info = gl::GetDirectCompositionHDRMonitorDXGIInfo();
  for (const auto& output_desc : dxgi_info->output_descs)
    hdr_enabled |= output_desc->hdr_enabled;

  ComD3D11Device d3d11_device;
  if (traits.media_gpu_channel_manager) {
    d3d11_device = traits.media_gpu_channel_manager->d3d11_device();
  }

  return D3D11VideoDecoder::Create(
      traits.gpu_task_runner, traits.media_log->Clone(), traits.gpu_preferences,
      *traits.gpu_workarounds, traits.get_command_buffer_stub_cb,
      GetD3D11DeviceCallback(std::move(d3d11_device)),
      traits.get_cached_configs_cb.Run(), hdr_enabled);
}

std::unique_ptr<AudioEncoder> CreatePlatformAudioEncoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  auto encoding_runner = base::ThreadPool::CreateCOMSTATaskRunner({});
  auto mf_encoder = std::make_unique<MFAudioEncoder>(encoding_runner);
  return std::make_unique<OffloadingAudioEncoder>(std::move(mf_encoder),
                                                  std::move(encoding_runner),
                                                  std::move(task_runner));
}

absl::optional<SupportedVideoDecoderConfigs>
GetPlatformSupportedVideoDecoderConfigs(
    base::WeakPtr<MediaGpuChannelManager> manager,
    gpu::GpuDriverBugWorkarounds gpu_workarounds,
    gpu::GpuPreferences gpu_preferences,
    const gpu::GPUInfo& gpu_info,
    base::OnceCallback<SupportedVideoDecoderConfigs()> get_vda_configs) {
  // This method must be called on the GPU main thread.
  SupportedVideoDecoderConfigs supported_configs;
  if (gpu_preferences.disable_accelerated_video_decode)
    return supported_configs;
  if (!gpu_workarounds.disable_d3d11_video_decoder) {
    if (!manager) {
      return supported_configs;
    }

    auto d3d11_device = manager.get()->d3d11_device();
    if (!d3d11_device) {
      return supported_configs;
    }
    supported_configs = D3D11VideoDecoder::GetSupportedVideoDecoderConfigs(
        gpu_preferences, gpu_workarounds,
        GetD3D11DeviceCallback(std::move(d3d11_device)));
  }
  return supported_configs;
}

std::unique_ptr<AudioDecoder> CreatePlatformAudioDecoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<MediaLog> media_log) {
  return MediaFoundationAudioDecoder::Create();
}

VideoDecoderType GetPlatformDecoderImplementationType(
    gpu::GpuDriverBugWorkarounds gpu_workarounds,
    gpu::GpuPreferences gpu_preferences,
    const gpu::GPUInfo& gpu_info) {
  return VideoDecoderType::kD3D11;
}

// There is no CdmFactory on windows, so just stub it out.
class CdmFactory {};
std::unique_ptr<CdmFactory> CreatePlatformCdmFactory(
    mojom::FrameInterfaceFactory* frame_interfaces) {
  return nullptr;
}

}  // namespace media
