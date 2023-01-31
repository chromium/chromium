// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/gpu_mojo_media_client.h"

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "media/base/audio_decoder.h"
#include "media/base/media_switches.h"
#include "media/base/offloading_audio_encoder.h"
#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
#include "media/filters/win/media_foundation_audio_decoder.h"
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
#include "media/gpu/ipc/service/vda_video_decoder.h"
#include "media/gpu/windows/d3d11_video_decoder.h"
#include "media/gpu/windows/mf_audio_encoder.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_angle_util_win.h"

namespace media {

namespace {

D3D11VideoDecoder::GetD3D11DeviceCB GetD3D11DeviceCallback() {
  return base::BindRepeating(
      []() { return gl::QueryD3D11DeviceObjectFromANGLE(); });
}

}  // namespace

std::unique_ptr<VideoDecoder> CreatePlatformVideoDecoder(
    VideoDecoderTraits& traits) {
  if (traits.gpu_workarounds->disable_d3d11_video_decoder) {
    if (traits.gpu_workarounds->disable_dxva_video_decoder ||
        !base::FeatureList::IsEnabled(kDXVAVideoDecoding)) {
      return nullptr;
    }
    return VdaVideoDecoder::Create(
        traits.task_runner, traits.gpu_task_runner, traits.media_log->Clone(),
        *traits.target_color_space, traits.gpu_preferences,
        *traits.gpu_workarounds, traits.get_command_buffer_stub_cb,
        VideoDecodeAccelerator::Config::OutputMode::ALLOCATE);
  }
  // Report that HDR is enabled if any display has HDR enabled.
  bool hdr_enabled = false;
  auto dxgi_info = gl::GetDirectCompositionHDRMonitorDXGIInfo();
  for (const auto& output_desc : dxgi_info->output_descs)
    hdr_enabled |= output_desc->hdr_enabled;
  return D3D11VideoDecoder::Create(
      traits.gpu_task_runner, traits.media_log->Clone(), traits.gpu_preferences,
      *traits.gpu_workarounds, traits.get_command_buffer_stub_cb,
      GetD3D11DeviceCallback(), traits.get_cached_configs_cb.Run(),
      hdr_enabled);
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
    gpu::GpuDriverBugWorkarounds gpu_workarounds,
    gpu::GpuPreferences gpu_preferences,
    const gpu::GPUInfo& gpu_info,
    base::OnceCallback<SupportedVideoDecoderConfigs()> get_vda_configs) {
  SupportedVideoDecoderConfigs supported_configs;
  if (gpu_preferences.disable_accelerated_video_decode)
    return supported_configs;
  if (!gpu_workarounds.disable_d3d11_video_decoder) {
    supported_configs = D3D11VideoDecoder::GetSupportedVideoDecoderConfigs(
        gpu_preferences, gpu_workarounds, GetD3D11DeviceCallback());
  } else if (!gpu_workarounds.disable_dxva_video_decoder) {
    supported_configs = std::move(get_vda_configs).Run();
  }
  return supported_configs;
}

std::unique_ptr<AudioDecoder> CreatePlatformAudioDecoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
  return MediaFoundationAudioDecoder::Create(std::move(task_runner));
#else   // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
  return nullptr;
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
}

VideoDecoderType GetPlatformDecoderImplementationType(
    gpu::GpuDriverBugWorkarounds gpu_workarounds,
    gpu::GpuPreferences gpu_preferences,
    const gpu::GPUInfo& gpu_info) {
  return gpu_workarounds.disable_d3d11_video_decoder ? VideoDecoderType::kVda
                                                     : VideoDecoderType::kD3D11;
}

// There is no CdmFactory on windows, so just stub it out.
class CdmFactory {};
std::unique_ptr<CdmFactory> CreatePlatformCdmFactory(
    mojom::FrameInterfaceFactory* frame_interfaces) {
  return nullptr;
}

}  // namespace media
