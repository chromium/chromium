// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/gpu_mojo_media_client.h"

#include <d3d11.h>
#include <d3d12.h>
#include <wrl.h>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/windows_version.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "media/base/audio_decoder.h"
#include "media/base/media_switches.h"
#include "media/base/offloading_audio_encoder.h"
#include "media/base/win/media_foundation_package_runtime_locator.h"
#include "media/base/win/mf_feature_checks.h"
#include "media/base/win/mf_initializer.h"
#include "media/filters/win/media_foundation_audio_decoder.h"
#include "media/gpu/ipc/service/media_gpu_channel_manager.h"
#include "media/gpu/windows/d3d11_video_decoder.h"
#include "media/gpu/windows/mf_audio_encoder.h"

namespace media {

class GpuMojoMediaClientWin final : public GpuMojoMediaClient {
 public:
  GpuMojoMediaClientWin(GpuMojoMediaClientTraits& traits)
      : GpuMojoMediaClientWin(
            // Grab SharedContextState before `traits` is consumed by
            // GpuMojoMediaClient().
            traits.media_gpu_channel_manager
                ? traits.media_gpu_channel_manager->GetSharedContextState()
                : nullptr,
            traits) {}

  ~GpuMojoMediaClientWin() final = default;

 protected:
  std::unique_ptr<VideoDecoder> CreatePlatformVideoDecoder(
      VideoDecoderTraits& traits) final {
    if (gpu_workarounds_.disable_d3d11_video_decoder) {
      return nullptr;
    }

    if (!multithread_protected_ &&
        IsDedicatedMediaServiceThreadEnabled(
            gpu_info_.gl_implementation_parts.angle)) {
      // Since the D3D11Device used for decoding is shared with
      // SkiaRenderer(ANGLE or Dawn), we need multithread protection turned on
      // to use it from another thread.
      Microsoft::WRL::ComPtr<ID3D11Multithread> multi_threaded;
      auto hr = d3d11_device_->QueryInterface(IID_PPV_ARGS(&multi_threaded));
      CHECK(SUCCEEDED(hr));
      multi_threaded->SetMultithreadProtected(TRUE);
      multithread_protected_ = true;
    }

    return D3D11VideoDecoder::Create(
        gpu_task_runner_, traits.media_log->Clone(), gpu_preferences_,
        gpu_workarounds_, traits.get_command_buffer_stub_cb,
        GetD3DDeviceCallback(), traits.get_cached_configs_cb.Run());
  }

  std::unique_ptr<AudioEncoder> CreatePlatformAudioEncoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner) final {
    auto encoding_runner = base::ThreadPool::CreateCOMSTATaskRunner({});
    auto mf_encoder = std::make_unique<MFAudioEncoder>(encoding_runner);
    return std::make_unique<OffloadingAudioEncoder>(std::move(mf_encoder),
                                                    std::move(encoding_runner),
                                                    std::move(task_runner));
  }

  std::optional<SupportedAudioDecoderConfigs>
  GetPlatformSupportedAudioDecoderConfigs() final {
    SupportedAudioDecoderConfigs audio_configs;

#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
    if (FindMediaFoundationPackageDecoder(AudioCodec::kAC4)) {
      audio_configs.emplace_back(AudioCodec::kAC4, AudioCodecProfile::kUnknown);
    }
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)

#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
    // MS preloaded Dolby's AC3,EAC3 decoder into Windows image, but from
    // Windows 11 build 25992, all of them will be removed and provided by Dolby
    // as codec packs.
    // Preloaded decoder dll is placed in SYSTEM folder and named as
    // 'DolbyDecMFT.dll'.
    base::FilePath dolby_dec_mft_path =
        base::PathService::CheckedGet(base::DIR_SYSTEM);
    dolby_dec_mft_path = dolby_dec_mft_path.AppendASCII("DolbyDecMFT.dll");
    bool has_legacy_dolby_ac3_eac3_mft = false;
    {
      // AC3/EAC3 decoder check needs to access file system, so allow scoped
      // blocking here.
      base::ScopedAllowBlocking allow_blocking;
      has_legacy_dolby_ac3_eac3_mft = base::PathExists(dolby_dec_mft_path);
    }
    if (has_legacy_dolby_ac3_eac3_mft ||
        FindMediaFoundationPackageDecoder(AudioCodec::kEAC3)) {
      audio_configs.emplace_back(AudioCodec::kAC3, AudioCodecProfile::kUnknown);
      audio_configs.emplace_back(AudioCodec::kEAC3,
                                 AudioCodecProfile::kUnknown);
    }
#endif  // BUILDFLAG (ENABLE_PLATFORM_AC3_EAC3_AUDIO)

    if (base::win::GetVersion() >= base::win::Version::WIN11_22H2 &&
        InitializeMediaFoundation()) {
      audio_configs.emplace_back(AudioCodec::kAAC, AudioCodecProfile::kXHE_AAC);
    }

    return audio_configs;
  }

  std::optional<SupportedVideoDecoderConfigs>
  GetPlatformSupportedVideoDecoderConfigs() final {
    // This method must be called on the GPU main thread.
    SupportedVideoDecoderConfigs supported_configs;
    if (gpu_preferences_.disable_accelerated_video_decode) {
      return supported_configs;
    }
    if (!gpu_workarounds_.disable_d3d11_video_decoder) {
      supported_configs = D3D11VideoDecoder::GetSupportedVideoDecoderConfigs(
          gpu_preferences_, gpu_workarounds_, GetD3DDeviceCallback());
    }
    return supported_configs;
  }

  std::unique_ptr<AudioDecoder> CreatePlatformAudioDecoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::unique_ptr<MediaLog> media_log) final {
    return MediaFoundationAudioDecoder::Create();
  }

  VideoDecoderType GetPlatformDecoderImplementationType() final {
    return VideoDecoderType::kD3D11;
  }

 private:
  GpuMojoMediaClientWin(
      scoped_refptr<gpu::SharedContextState> shared_context_state,
      GpuMojoMediaClientTraits& traits)
      : GpuMojoMediaClient(traits) {
    // Note: `traits` is empty after GpuMojoMediaClient().
    if (!shared_context_state) {
      return;
    }

    d3d11_device_ = shared_context_state->GetD3D11Device();
    if (base::FeatureList::IsEnabled(kD3D12VideoDecoder)) {
      Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
      CHECK_EQ(d3d11_device_.As(&dxgi_device), S_OK);
      Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
      CHECK_EQ(dxgi_device->GetAdapter(&adapter), S_OK);
      d3d12_device_ = CreateD3D12Device(adapter.Get());
    }
  }

  D3D11VideoDecoder::GetD3DDeviceCB GetD3DDeviceCallback() {
    return base::BindRepeating(
        [](Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
           Microsoft::WRL::ComPtr<ID3D12Device> d3d12_device,
           D3D11VideoDecoder::D3DVersion d3d_version)
            -> Microsoft::WRL::ComPtr<IUnknown> {
          if (d3d_version == D3D11VideoDecoder::D3DVersion::kD3D11) {
            return d3d11_device;
          } else if (d3d_version == D3D11VideoDecoder::D3DVersion::kD3D12) {
            return d3d12_device;
          }
          NOTREACHED();
        },
        d3d11_device_, d3d12_device_);
  }

  bool multithread_protected_ = false;
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  Microsoft::WRL::ComPtr<ID3D12Device> d3d12_device_;
};

std::unique_ptr<GpuMojoMediaClient> CreateGpuMediaService(
    GpuMojoMediaClientTraits& traits) {
  return std::make_unique<GpuMojoMediaClientWin>(traits);
}

}  // namespace media
