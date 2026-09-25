// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_video_decoder_backend.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/windows/d3d11_texture_selector.h"
#include "media/gpu/windows/d3d11_video_decoder_wrapper.h"
#include "media/gpu/windows/d3d11_video_device_format_support.h"
#include "media/gpu/windows/d3d12_video_decoder_wrapper.h"
#include "media/gpu/windows/d3d_decoder_configurator.h"
#include "media/gpu/windows/d3d_picture_buffer.h"
#include "media/gpu/windows/supported_profile_helpers.h"
#include "media/media_buildflags.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {
bool IsFeatureLevelSupported(ComD3D11Device device) {
  return device && device->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0;
}
}  // namespace

D3D11VideoDecoderBackend::D3D11VideoDecoderBackend() = default;

D3D11VideoDecoderBackend::~D3D11VideoDecoderBackend() {
  // Drivers may cache released D3D11 resources. Flush to make that less likely.
  if (device_context_) {
    device_context_->Flush();
  }
}

std::vector<SupportedVideoDecoderConfig>
D3D11VideoDecoderBackend::GetSupportedVideoDecoderConfigs(
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    GetD3DDeviceCB get_d3d_device_cb) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (gpu_workarounds.disable_d3d11_video_decoder) {
    return {};
  }

  SupportedResolutionRangeMap supported_resolutions;
  if (base::FeatureList::IsEnabled(kD3D12VideoDecoder)) {
    ComUnknown d3d_device = get_d3d_device_cb.Run(D3DVersion::kD3D12);
    if (!d3d_device) {
      return {};
    }
    ComD3D12Device d3d12_device;
    CHECK_EQ(d3d_device.As(&d3d12_device), S_OK);

    // D3D11 fences synchronize the D3D12 decoder with D3D11 output resources.
    d3d_device = get_d3d_device_cb.Run(D3DVersion::kD3D11);
    if (!d3d_device) {
      return {};
    }
    ComD3D11Device d3d11_device;
    CHECK_EQ(d3d_device.As(&d3d11_device), S_OK);
    ComD3D11Device5 d3d11_device5;
    if (d3d11_device.As(&d3d11_device5) != S_OK) {
      return {};
    }

    supported_resolutions =
        GetSupportedD3D12VideoDecoderResolutions(d3d12_device, gpu_workarounds);
  } else {
    // Remember that this might query the ANGLE device, so this won't work if
    // we're not on the GPU main thread. Devices are thread safe (contexts are
    // not), so a cached device could be used from another thread.
    ComUnknown d3d_device = get_d3d_device_cb.Run(D3DVersion::kD3D11);
    if (!d3d_device) {
      return {};
    }
    ComD3D11Device d3d11_device;
    CHECK_EQ(d3d_device.As(&d3d11_device), S_OK);

    if (!IsFeatureLevelSupported(d3d11_device)) {
      return {};
    }

    supported_resolutions =
        GetSupportedD3D11VideoDecoderResolutions(d3d11_device, gpu_workarounds);
  }

  std::vector<SupportedVideoDecoderConfig> configs;
  for (const auto& [profile, resolution_range] : supported_resolutions) {
    DCHECK(!resolution_range.min_resolution.IsEmpty());
    DCHECK(!resolution_range.max_landscape_resolution.IsEmpty());

    configs.emplace_back(profile, profile, resolution_range.min_resolution,
                         resolution_range.max_landscape_resolution,
                         /*allow_encrypted=*/false,
                         /*require_encrypted=*/false);
    if (resolution_range.max_portrait_resolution) {
      configs.emplace_back(profile, profile, resolution_range.min_resolution,
                           *resolution_range.max_portrait_resolution,
                           /*allow_encrypted=*/false,
                           /*require_encrypted=*/false);
    }
  }

  return configs;
}

D3DStatus D3D11VideoDecoderBackend::AcquireDeviceResources(
    GetD3DDeviceCB get_d3d_device_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The current decoder uses the ANGLE device and its immediate context.
  ComUnknown d3d_device = get_d3d_device_cb.Run(D3DVersion::kD3D11);
  if (!d3d_device) {
    return D3DStatus::Codes::kFailedToGetAngleDevice;
  }
  CHECK_EQ(d3d_device.As(&device_), S_OK);

  if (!IsFeatureLevelSupported(device_)) {
    return D3DStatus::Codes::kUnsupportedFeatureLevel;
  }

  device_->GetImmediateContext(&device_context_);

  HRESULT hr = device_.As(&video_device_);
  if (FAILED(hr)) {
    return {D3DStatus::Codes::kFailedToGetVideoDevice, hr};
  }

  return D3DStatus::Codes::kOk;
}

void D3D11VideoDecoderBackend::LogDecoderAdapterInfo(MediaLog* media_log) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_) {
    return;
  }

  ComDXGIDevice dxgi_device;
  HRESULT hr = device_.As(&dxgi_device);
  if (FAILED(hr)) {
    return;
  }

  ComDXGIAdapter dxgi_adapter;
  hr = dxgi_device->GetAdapter(&dxgi_adapter);
  CHECK_EQ(hr, S_OK);

  DXGI_ADAPTER_DESC adapter_desc{};
  hr = dxgi_adapter->GetDesc(&adapter_desc);
  if (FAILED(hr)) {
    return;
  }

  MEDIA_LOG(INFO, media_log) << "Selected D3DVideoDecoder adapter LUID:{"
                             << adapter_desc.AdapterLuid.HighPart << ", "
                             << adapter_desc.AdapterLuid.LowPart << "}";
}

D3DStatus::Or<std::unique_ptr<D3DDecoderConfigurator>>
D3D11VideoDecoderBackend::CreateDecoderConfigurator(
    uint8_t bit_depth,
    const VideoDecoderConfig& config,
    VideoChromaSampling chroma_sampling,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    bool use_shared_handle,
    MediaLog* media_log) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto decoder_configurator = D3DDecoderConfigurator::Create(
      gpu_preferences, gpu_workarounds, config, bit_depth, chroma_sampling,
      media_log, use_shared_handle, device_);
  if (!decoder_configurator) {
    return D3DStatus::Codes::kDecoderUnsupportedProfile;
  }

  if (!decoder_configurator->SupportsD3D11Device(video_device_)) {
    return D3DStatus::Codes::kDecoderUnsupportedCodec;
  }

  return decoder_configurator;
}

D3DStatus::Or<std::unique_ptr<TextureSelector>>
D3D11VideoDecoderBackend::CreateTextureSelector(
    D3DDecoderConfigurator* decoder_configurator,
    const VideoDecoderConfig& config,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    bool use_shared_handle,
    MediaLog* media_log) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(decoder_configurator);

  FormatSupportChecker format_checker(device_);
  if (!format_checker.Initialize()) {
    // Do not fail; unsupported formats will simply be rejected below.
    MEDIA_LOG(WARNING, media_log)
        << "Could not create format checker, continuing";
  }

  auto texture_selector = TextureSelector::Create(
      gpu_preferences, gpu_workarounds, decoder_configurator->TextureFormat(),
      &format_checker, video_device_, device_context_, media_log,
      config.color_space_info().ToGfxColorSpace(), use_shared_handle);
  if (!texture_selector) {
    return D3DStatus::Codes::kCreateTextureSelectorFailed;
  }

  return texture_selector;
}

D3DStatus::Or<std::unique_ptr<D3DVideoDecoderWrapper>>
D3D11VideoDecoderBackend::CreateVideoDecoderWrapper(
    GetD3DDeviceCB get_d3d_device_cb,
    D3DDecoderConfigurator* decoder_configurator,
    const VideoDecoderConfig& config,
    uint8_t bit_depth,
    VideoChromaSampling chroma_sampling,
    int max_decode_requests,
    MediaLog* media_log) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(decoder_configurator);

  if (base::FeatureList::IsEnabled(kD3D12VideoDecoder)) {
    MEDIA_LOG(INFO, media_log) << "D3DVideoDecoder is using D3D12 backend";
    // TODO(liberato): On re-init, we can probably reuse the device.
    ComUnknown d3d_device = get_d3d_device_cb.Run(D3DVersion::kD3D12);
    if (!d3d_device) {
      return {D3DStatusCode::kUnsupportedFeatureLevel,
              "Cannot create D3D12Device"};
    }

    ComD3D12Device d3d12_device;
    CHECK_EQ(d3d_device.As(&d3d12_device), S_OK);

    ComD3D12VideoDevice d3d12_video_device;
    HRESULT hr = d3d12_device.As(&d3d12_video_device);
    if (FAILED(hr)) {
      return D3DStatus{D3DStatusCode::kFailedToGetVideoDevice,
                       "Cannot create D3D12VideoDevice", hr};
    }

    // Check ID3D11Device5 is supported so that we can use D3D11Fence.
    d3d_device = get_d3d_device_cb.Run(D3DVersion::kD3D11);
    if (!d3d_device) {
      return {D3DStatusCode::kUnsupportedFeatureLevel,
              "Cannot create D3D11Device"};
    }
    ComD3D11Device5 d3d11_device5;
    if (d3d_device.As(&d3d11_device5) != S_OK) {
      return {D3DStatusCode::kUnsupportedFeatureLevel,
              "Cannot get ID3D11Device5 interface"};
    }

    std::unique_ptr<D3DVideoDecoderWrapper> video_decoder_wrapper =
        D3D12VideoDecoderWrapper::Create(media_log, d3d12_video_device, config,
                                         bit_depth, chroma_sampling,
                                         max_decode_requests);
    return video_decoder_wrapper;
  }

  MEDIA_LOG(INFO, media_log) << "D3DVideoDecoder is using D3D11 backend";
  ComD3D11VideoContext1 video_context;
  CHECK_EQ(device_context_.As(&video_context), S_OK);
  std::unique_ptr<D3DVideoDecoderWrapper> video_decoder_wrapper =
      D3D11VideoDecoderWrapper::Create(media_log, video_device_,
                                       std::move(video_context),
                                       decoder_configurator, config);
  return video_decoder_wrapper;
}

std::unique_ptr<Texture2DWrapper>
D3D11VideoDecoderBackend::CreateOutputTextureWrapper(
    TextureSelector* texture_selector,
    const gfx::ColorSpace& color_space,
    const gfx::Size& size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(texture_selector);
  return texture_selector->CreateTextureWrapper(device_, color_space, size);
}

D3DStatus::Or<ComD3D11Texture2D>
D3D11VideoDecoderBackend::CreateDecoderOutputTexture(
    const gfx::Size& size,
    uint32_t array_size,
    D3DDecoderConfigurator* decoder_configurator,
    TextureSelector* texture_selector) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(decoder_configurator);
  CHECK(texture_selector);
  return decoder_configurator->CreateD3D11OutputTexture(
      device_, size, array_size,
      texture_selector->DoesDecoderOutputUseSharedHandle());
}

D3DStatus::Or<scoped_refptr<D3DPictureBuffer>>
D3D11VideoDecoderBackend::CreateAndInitPictureBuffer(
    const gfx::Size& size,
    uint32_t array_size,
    size_t array_slice,
    size_t picture_index,
    bool use_single_video_decoder_texture,
    std::unique_ptr<Texture2DWrapper> texture_wrapper,
    D3DDecoderConfigurator* decoder_configurator,
    TextureSelector* texture_selector,
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()> get_helper_cb,
    MediaLog* media_log,
    base::OnceCallback<void(scoped_refptr<D3DPictureBuffer>)> init_done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(texture_wrapper);
  CHECK(decoder_configurator);
  CHECK(texture_selector);

  if (picture_index == 0) {
    picture_buffer_texture_.Reset();
  }

  ComD3D11Texture2D texture;
  if (use_single_video_decoder_texture) {
    auto result = CreateDecoderOutputTexture(
        size, /*array_size=*/1, decoder_configurator, texture_selector);
    if (!result.has_value()) {
      return std::move(result).error().AddHere();
    }
    texture = std::move(result).value();
  } else {
    if (!picture_buffer_texture_) {
      auto result = CreateDecoderOutputTexture(
          size, array_size, decoder_configurator, texture_selector);
      if (!result.has_value()) {
        return std::move(result).error().AddHere();
      }
      picture_buffer_texture_ = std::move(result).value();
    }
    texture = picture_buffer_texture_;
  }

  auto picture_buffer = base::MakeRefCounted<D3DPictureBuffer>(
      std::move(decoder_task_runner), std::move(texture), array_slice,
      std::move(texture_wrapper), picture_index);

  // Keep the decoder idle until GPU-side shared-image initialization finishes.
  // `add_client_use()` must precede Init(). If Init() fails after the texture
  // wrapper posted its GPU-thread work, the init-done callback still runs and
  // calls remove_client_use(); adding the ref later would underflow it.
  picture_buffer->add_client_use();
  D3DStatus result =
      picture_buffer->Init(std::move(gpu_task_runner), std::move(get_helper_cb),
                           video_device_, decoder_configurator->DecoderGuid(),
                           media_log->Clone(), std::move(init_done_cb));
  if (!result.is_ok()) {
    return std::move(result).AddHere();
  }

  return picture_buffer;
}

D3DStatus D3D11VideoDecoderBackend::WaitForDecodeComplete(
    D3DPictureBuffer* picture_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(picture_buffer);
  return picture_buffer->WaitForDecodeCompleteGPU(device_context_.Get());
}

bool D3D11VideoDecoderBackend::ShouldUseDXVADeviceForHEVCRangeExtension(
    const VideoDecoderConfig& config) const {
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return config.profile() == HEVCPROFILE_REXT &&
         (base::FeatureList::IsEnabled(kD3D12VideoDecoder) ||
          SupportsHEVCRangeExtensionDXVAProfile(device_));
#else
  return false;
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
}

VideoDecoderType D3D11VideoDecoderBackend::GetDecoderType() const {
  return VideoDecoderType::kD3D11;
}

}  // namespace media
