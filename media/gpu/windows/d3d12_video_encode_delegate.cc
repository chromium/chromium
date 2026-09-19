// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_video_encode_delegate.h"

#include <algorithm>
#include <map>
#include <optional>
#include <ranges>
#include <utility>

#include "base/bits.h"
#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/base/win/mf_helpers.h"
#include "media/gpu/gpu_video_encode_accelerator_helpers.h"
#include "media/gpu/h264_dpb.h"
#include "media/gpu/windows/d3d12_helpers.h"
#include "media/gpu/windows/d3d12_video_encode_av1_delegate.h"
#include "media/gpu/windows/d3d12_video_encode_h264_delegate.h"
#include "media/gpu/windows/d3d12_video_encoder_wrapper.h"
#include "media/gpu/windows/format_utils.h"
#include "third_party/microsoft_dxheaders/src/include/directx/d3dx12_core.h"
#include "ui/gfx/color_space_win.h"

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
#include "media/gpu/windows/d3d12_video_encode_h265_delegate.h"
#include "media/parsers/h265_parser.h"
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

#define CHECK_FEATURE_SUPPORT(feature_suffix, data)                         \
  do {                                                                      \
    HRESULT hr = video_device->CheckFeatureSupport(                         \
        D3D12_FEATURE_VIDEO_ENCODER_##feature_suffix, &data, sizeof(data)); \
    if (FAILED(hr)) {                                                       \
      LOG(ERROR) << "CheckFeatureSupport for " #feature_suffix " failed: "  \
                 << PrintHr(hr);                                            \
      return {};                                                            \
    }                                                                       \
  } while (0)

namespace media {

namespace {

// How many luma samples share one chroma sample, horizontally and vertically.
struct ChromaSubsampling {
  int x = 1;
  int y = 1;
};

ChromaSubsampling GetChromaSubsampling(DXGI_FORMAT format) {
  switch (format) {
    // 4:2:0: chroma is shared both horizontally and vertically.
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
    case DXGI_FORMAT_420_OPAQUE:
      return {2, 2};
    // 4:2:2: chroma is shared horizontally only.
    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216:
    case DXGI_FORMAT_YUY2:
      return {2, 1};
    // 4:4:4 and RGB have a chroma sample per pixel, so any crop is expressible.
    default:
      return {1, 1};
  }
}

bool IsVBRSupported(ID3D12VideoDevice3* video_device,
                    VideoCodecProfile output_profile) {
  D3D12_VIDEO_ENCODER_CODEC codec;
  switch (VideoCodecProfileToVideoCodec(output_profile)) {
    case VideoCodec::kH264:
      codec = D3D12_VIDEO_ENCODER_CODEC_H264;
      break;
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    case VideoCodec::kHEVC:
      codec = D3D12_VIDEO_ENCODER_CODEC_HEVC;
      break;
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    case VideoCodec::kAV1:
      codec = D3D12_VIDEO_ENCODER_CODEC_AV1;
      break;
    default:
      return false;
  }

  D3D12_FEATURE_DATA_VIDEO_ENCODER_RATE_CONTROL_MODE vbr{
      .Codec = codec,
      .RateControlMode = D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_VBR,
  };

  HRESULT hr = video_device->CheckFeatureSupport(
      D3D12_FEATURE_VIDEO_ENCODER_RATE_CONTROL_MODE, &vbr, sizeof(vbr));

  return SUCCEEDED(hr) && vbr.IsSupported;
}

// Returns the DXGI format the encoder should consume for |output_profile|, or
// std::nullopt if |input_format| cannot be encoded into that profile.
std::optional<DXGI_FORMAT> GetDxgiInputFormat(VideoCodecProfile output_profile,
                                              VideoPixelFormat input_format) {
  // H.264 high10 and HEVC main10 are 10 bit by definition, while AV1 main
  // covers both 8 and 10 bit, so for AV1 main the input format is what decides
  // the coded bit depth.
  if (output_profile == H264PROFILE_HIGH10PROFILE ||
      output_profile == HEVCPROFILE_MAIN10 ||
      (output_profile == AV1PROFILE_PROFILE_MAIN &&
       input_format == PIXEL_FORMAT_P010LE)) {
    return DXGI_FORMAT_P010;
  } else if (output_profile == AV1PROFILE_PROFILE_HIGH) {
    return DXGI_FORMAT_AYUV;
  } else if (output_profile == HEVCPROFILE_REXT) {
    // The input format selects the range extension profile variant and thus
    // the DXGI format of the coded stream. Only the 10 bit variants are
    // supported; 8 bit range extension uses the upstream 4:4:4 path.
    switch (input_format) {
      case PIXEL_FORMAT_P210LE:
        return DXGI_FORMAT_Y210;  // main10_422, 10 bit 4:2:2
      case PIXEL_FORMAT_P410LE:
        return DXGI_FORMAT_Y410;  // main10_444, 10 bit 4:4:4
      default:
        return std::nullopt;
    }
  } else {
    return DXGI_FORMAT_NV12;
  }
}

// The shared image formats the encoder advertises as GPU inputs, with the
// DXGI format the video processor sees them as. These follow the texture
// formats Chromium actually creates on Windows (D3DImageBackingFactory): the
// B-first 32bpp formats are B8G8R8A8 and the R-first ones R8G8B8A8 textures,
// with the X channel carried as opaque alpha — the D3D shared image factory
// creates no X8 layouts.
constexpr auto kGpuSharedImageFormatCandidates =
    base::MakeFixedFlatMap<VideoPixelFormat, DXGI_FORMAT>({
        {PIXEL_FORMAT_NV12, DXGI_FORMAT_NV12},
        {PIXEL_FORMAT_P010LE, DXGI_FORMAT_P010},
        {PIXEL_FORMAT_ARGB, DXGI_FORMAT_B8G8R8A8_UNORM},
        {PIXEL_FORMAT_XRGB, DXGI_FORMAT_B8G8R8A8_UNORM},
        {PIXEL_FORMAT_ABGR, DXGI_FORMAT_R8G8B8A8_UNORM},
        {PIXEL_FORMAT_XBGR, DXGI_FORMAT_R8G8B8A8_UNORM},
        {PIXEL_FORMAT_XB30, DXGI_FORMAT_R10G10B10A2_UNORM},
        {PIXEL_FORMAT_RGBAF16, DXGI_FORMAT_R16G16B16A16_FLOAT},
    });

// Whether the video processor can convert |input_format| to |output_format|.
// Probed at a representative resolution with BT.709; format pair support does
// not depend on the resolution or the color space.
bool IsVideoProcessorFormatPairSupported(ID3D12VideoDevice3* video_device,
                                         DXGI_FORMAT input_format,
                                         DXGI_FORMAT output_format) {
  const gfx::ColorSpace rec709 = gfx::ColorSpace::CreateREC709();
  D3D12_FEATURE_DATA_VIDEO_PROCESS_SUPPORT support{
      .InputSample = {.Width = 1280,
                      .Height = 720,
                      .Format = {.Format = input_format,
                                 .ColorSpace =
                                     gfx::ColorSpaceWin::GetDXGIColorSpace(
                                         rec709)}},
      .InputFrameRate = {30, 1},
      .OutputFormat = {.Format = output_format,
                       .ColorSpace =
                           gfx::ColorSpaceWin::GetDXGIColorSpace(rec709)},
      .OutputFrameRate = {30, 1},
  };
  HRESULT hr = video_device->CheckFeatureSupport(
      D3D12_FEATURE_VIDEO_PROCESS_SUPPORT, &support, sizeof(support));
  return SUCCEEDED(hr) &&
         support.SupportFlags == D3D12_VIDEO_PROCESS_SUPPORT_FLAG_SUPPORTED;
}

}  // namespace

// static
VideoEncodeAccelerator::SupportedProfiles
D3D12VideoEncodeDelegate::GetSupportedProfiles(
    ID3D12VideoDevice3* video_device,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    const std::vector<D3D12_VIDEO_ENCODER_CODEC>& codecs) {
  CHECK(video_device);
  // Video processor support is queried per (candidate, encoder input) DXGI
  // format pair, and the same pair recurs across profiles and codecs. Cache
  // the results for this call: the device does not change within it, so each
  // pair only needs one probe.
  std::map<std::pair<DXGI_FORMAT, DXGI_FORMAT>, bool> vp_support_cache;
  auto supports_vp_format_pair = [&](DXGI_FORMAT source_format,
                                     DXGI_FORMAT target_format) {
    const auto [it, inserted] = vp_support_cache.try_emplace(
        std::make_pair(source_format, target_format), false);
    if (inserted) {
      it->second = IsVideoProcessorFormatPairSupported(
          video_device, source_format, target_format);
    }
    return it->second;
  };
  VideoEncodeAccelerator::SupportedProfiles supported_profiles;
  for (D3D12_VIDEO_ENCODER_CODEC codec : codecs) {
    D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC codec_support{.Codec = codec};
    CHECK_FEATURE_SUPPORT(CODEC, codec_support);
    if (!codec_support.IsSupported) {
      continue;
    }
    VideoEncodeAccelerator::SupportedProfile supported_profile;
    D3D12_FEATURE_DATA_VIDEO_ENCODER_OUTPUT_RESOLUTION_RATIOS_COUNT count{
        .Codec = codec,
    };
    CHECK_FEATURE_SUPPORT(OUTPUT_RESOLUTION_RATIOS_COUNT, count);
    std::vector<D3D12_VIDEO_ENCODER_PICTURE_RESOLUTION_RATIO_DESC> ratios(
        count.ResolutionRatiosCount);
    D3D12_FEATURE_DATA_VIDEO_ENCODER_OUTPUT_RESOLUTION output_resolution{
        .Codec = codec,
        .ResolutionRatiosCount = count.ResolutionRatiosCount,
        .pResolutionRatios = ratios.data(),
    };
    CHECK_FEATURE_SUPPORT(OUTPUT_RESOLUTION, output_resolution);
    if (!output_resolution.IsSupported) {
      continue;
    }
    supported_profile.min_resolution =
        gfx::Size(output_resolution.MinResolutionSupported.Width,
                  output_resolution.MinResolutionSupported.Height);
    supported_profile.max_resolution =
        gfx::Size(output_resolution.MaxResolutionSupported.Width,
                  output_resolution.MaxResolutionSupported.Height);
    supported_profile.max_framerate_numerator = 30;
    supported_profile.max_framerate_denominator = 1;
    D3D12_FEATURE_DATA_VIDEO_ENCODER_RATE_CONTROL_MODE cbr{
        .Codec = codec,
        .RateControlMode = D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CBR,
    };
    CHECK_FEATURE_SUPPORT(RATE_CONTROL_MODE, cbr);
    D3D12_FEATURE_DATA_VIDEO_ENCODER_RATE_CONTROL_MODE cqp{
        .Codec = codec,
        .RateControlMode = D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CQP,
    };
    CHECK_FEATURE_SUPPORT(RATE_CONTROL_MODE, cqp);
    // If VBR is not supported, we will fallback to CBR.
    supported_profile.rate_control_modes =
        (cbr.IsSupported ? VideoEncodeAccelerator::kConstantMode |
                               VideoEncodeAccelerator::kVariableMode
                         : VideoEncodeAccelerator::kNoMode) |
        (cqp.IsSupported ? VideoEncodeAccelerator::kExternalMode
                         : VideoEncodeAccelerator::kNoMode);
    supported_profile.scalability_modes = {
        SVCScalabilityMode::kL1T1,
        SVCScalabilityMode::kL1T2,
    };
    if (base::FeatureList::IsEnabled(kD3D12VideoEncodeAcceleratorL1T3) &&
        !(codec == D3D12_VIDEO_ENCODER_CODEC_AV1 &&
          gpu_workarounds.disable_d3d12_av1_multi_ref_encoding)) {
      supported_profile.scalability_modes.push_back(SVCScalabilityMode::kL1T3);
    }
    supported_profile.is_software_codec = false;

    std::vector<std::pair<VideoCodecProfile, std::vector<VideoPixelFormat>>>
        profiles;
    switch (codec) {
      case D3D12_VIDEO_ENCODER_CODEC_H264:
        profiles =
            D3D12VideoEncodeH264Delegate::GetSupportedProfiles(video_device);
        break;
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
      case D3D12_VIDEO_ENCODER_CODEC_HEVC:
        profiles =
            D3D12VideoEncodeH265Delegate::GetSupportedProfiles(video_device);
        break;
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
      case D3D12_VIDEO_ENCODER_CODEC_AV1:
        profiles = D3D12VideoEncodeAV1Delegate::GetSupportedProfiles(
            video_device, gpu_workarounds);
        break;
      default:
        NOTREACHED();
    }
    bool supports_shared_image =
        base::FeatureList::IsEnabled(kD3D12SharedImageEncode);
    for (const auto& [profile, formats] : profiles) {
      supported_profile.profile = profile;
      supported_profile.gpu_supported_pixel_formats = formats;
      // The range extension profile variant is identified by its input format,
      // so report its chroma subsampling and bit depth for the client to match
      // encode options, like the macOS encoder does. The regular profiles
      // leave both fields empty.
      if (profile == HEVCPROFILE_REXT) {
        supported_profile.chroma_sampling =
            VideoPixelFormatToChromaSampling(formats[0]);
        supported_profile.bit_depth =
            base::checked_cast<uint8_t>(BitDepth(formats[0]));
      } else {
        supported_profile.chroma_sampling = std::nullopt;
        supported_profile.bit_depth = std::nullopt;
      }
      if (supports_shared_image) {
        std::vector<VideoPixelFormat> shared_image_formats;
        const DXGI_FORMAT encoder_input_format =
            GetDxgiInputFormat(profile, formats[0])
                .value_or(DXGI_FORMAT_UNKNOWN);
        if (encoder_input_format != DXGI_FORMAT_UNKNOWN) {
          for (const auto& [pixel_format, dxgi_format] :
               kGpuSharedImageFormatCandidates) {
            if (dxgi_format == encoder_input_format ||
                supports_vp_format_pair(dxgi_format, encoder_input_format)) {
              shared_image_formats.push_back(pixel_format);
            }
          }
        }
        supported_profile.supports_gpu_shared_images =
            !shared_image_formats.empty();
        std::ranges::copy(
            shared_image_formats,
            std::back_inserter(supported_profile.gpu_supported_pixel_formats));
      }
      supported_profiles.push_back(supported_profile);
    }
  }
  return supported_profiles;
}

D3D12VideoEncodeDelegate::D3D12VideoEncodeDelegate(
    Microsoft::WRL::ComPtr<ID3D12VideoDevice3> video_device,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds)
    : video_device_(std::move(video_device)),
      gpu_workarounds_(gpu_workarounds) {
  CHECK(video_device_);
}

D3D12VideoEncodeDelegate::~D3D12VideoEncodeDelegate() = default;

EncoderStatus D3D12VideoEncodeDelegate::Initialize(
    VideoEncodeAccelerator::Config config) {
  CHECK_EQ(video_device_.As(&device_), S_OK);

  Microsoft::WRL::ComPtr<ID3D12VideoDevice1> video_device1;
  CHECK_EQ(video_device_.As(&video_device1), S_OK);
  video_processor_wrapper_ =
      video_processor_wrapper_factory_.Run(video_device1);

  output_profile_ = config.output_profile;

  if (!config.manual_reference_buffer_control) {
    svc_layers_.emplace(
        SVCLayers::Config({config.input_visible_size}, 0, 1,
                          config.spatial_layers.empty()
                              ? 1
                              : config.spatial_layers[0].num_of_temporal_layers,
                          SVCInterLayerPredMode::kOff));
  } else {
    svc_layers_.reset();
  }

  input_size_.Width = config.input_visible_size.width();
  input_size_.Height = config.input_visible_size.height();

  std::optional<DXGI_FORMAT> input_format =
      GetDxgiInputFormat(output_profile_, config.input_format);
  if (!input_format) {
    return {EncoderStatus::Codes::kEncoderUnsupportedConfig,
            base::StrCat({"Input format ",
                          VideoPixelFormatToString(config.input_format),
                          " cannot be encoded into profile ",
                          GetProfileName(output_profile_)})};
  }
  input_format_ = *input_format;
  processed_input_frame_.Reset();

  // A packed 4:2:2 macro-pixel covers two horizontally adjacent luma samples,
  // so an odd width has no representation in the encoder input surface. 4:2:0
  // formats pad their half-resolution chroma planes instead and so are not
  // constrained here; odd crops are still rejected per frame in Encode().
  const ChromaSubsampling input_subsampling =
      GetChromaSubsampling(input_format_);
  if (input_subsampling.x == 2 && input_subsampling.y == 1 &&
      config.input_visible_size.width() % 2 != 0) {
    return {
        EncoderStatus::Codes::kEncoderUnsupportedConfig,
        base::StrCat({"Odd input width ",
                      base::NumberToString(config.input_visible_size.width()),
                      " is not supported for 4:2:2 encoding"})};
  }

  bitrate_allocation_ = AllocateBitrateForDefaultEncoding(config);
  framerate_ = config.framerate;
  rate_control_ = D3D12VideoEncoderRateControl::Create(
      bitrate_allocation_, config.framerate, video_device_.Get(),
      output_profile_);

  // The encoder wrapper will use this for allocating the bitstream buffer,
  // make it at least as large as the estimated bitstream size and the size
  // of an uncompressed frame, whichever is larger.
  min_bitstream_buffer_size_ = std::max(
      static_cast<uint64_t>(VideoEncodeAccelerator::EstimateBitstreamBufferSize(
          config.bitrate, config.framerate, config.input_format,
          config.input_visible_size)),
      static_cast<uint64_t>(VideoFrame::AllocationSize(
          config.input_format, config.input_visible_size)));

  static constexpr uint32_t kDefaultGOPLength = 3000;
  config.gop_length = config.gop_length.value_or(kDefaultGOPLength);

  if (!video_processor_wrapper_->Init()) {
    return EncoderStatus::Codes::kEncoderInitializationError;
  }

  return InitializeVideoEncoder(config);
}

bool D3D12VideoEncodeDelegate::ReportsAverageQp() const {
  return false;
}

bool D3D12VideoEncodeDelegate::UpdateRateControl(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate) {
  auto rate_control = D3D12VideoEncoderRateControl::Create(
      bitrate_allocation, framerate, video_device_.Get(), output_profile_);

  if (rate_control.GetMode() != rate_control_.GetMode() &&
      !SupportsRateControlReconfiguration()) {
    return false;
  }

  bitrate_allocation_ = bitrate_allocation;
  framerate_ = framerate;
  rate_control_ = rate_control;
  return true;
}

EncoderStatus::Or<D3D12VideoEncodeDelegate::EncodeResult>
D3D12VideoEncodeDelegate::Encode(
    D3D12PictureBuffer picture_buffer,
    const gfx::Rect& input_visible_rect,
    const gfx::ColorSpace& input_frame_color_space,
    const BitstreamBuffer& bitstream_buffer,
    const VideoEncoder::EncodeOptions& options,
    const gfx::HDRMetadata& input_frame_hdr_metadata) {
  if (options.reference_buffers.size() > GetMaxNumOfManualRefBuffers()) {
    return {EncoderStatus::Codes::kBadReferenceBuffer,
            "Number of manual reference buffers exceeds that is supported by "
            "encoder"};
  }
  if (!svc_layers_ && !options.key_frame && options.reference_buffers.empty()) {
    return {EncoderStatus::Codes::kBadReferenceBuffer,
            "Non-keyframe must have at least one reference buffer"};
  }

  // Validate reference buffer indices early, before submitting any GPU work
  // (e.g., video processing). This prevents a scenario where GPU commands are
  // submitted but the subsequent CPU synchronization is skipped due to an
  // error in EncodeImpl().
  for (uint8_t ref_idx : options.reference_buffers) {
    if (ref_idx >= GetMaxNumOfManualRefBuffers()) {
      return {EncoderStatus::Codes::kBadReferenceBuffer,
              "Manual reference buffer index exceeds that is supported by "
              "encoder"};
    }
  }
  if (options.update_buffer.has_value() &&
      options.update_buffer.value() >= GetMaxNumOfManualRefBuffers()) {
    return {EncoderStatus::Codes::kBadReferenceBuffer,
            "Update buffer index is out of range"};
  }

  const gfx::ColorSpace& output_color_space =
      GetEncoderOutputColorSpaceFromInputColorSpace(input_frame_color_space);

  const D3D12_RESOURCE_DESC input_frame_desc =
      picture_buffer.resource->GetDesc();
  const gfx::Rect input_frame_rect(
      0, 0, base::checked_cast<int>(input_frame_desc.Width),
      base::checked_cast<int>(input_frame_desc.Height));
  if (input_visible_rect.IsEmpty() ||
      !input_frame_rect.Contains(input_visible_rect)) {
    return {EncoderStatus::Codes::kInvalidInputFrame,
            base::StrCat({"Input frame visible rectangle ",
                          input_visible_rect.ToString(),
                          " is not a valid region of the input texture ",
                          input_frame_rect.ToString()})};
  }
  // Subsampled chroma is shared between neighboring rows and/or columns, so an
  // odd origin or size along a shared axis does not name a whole chroma sample
  // and cannot be cropped to. Reject it rather than silently encoding a
  // different region, matching MF VEA's behavior. 4:2:2 shares along x only,
  // so its height is unconstrained.
  const ChromaSubsampling subsampling =
      GetChromaSubsampling(input_frame_desc.Format);
  if (input_visible_rect.x() % subsampling.x != 0 ||
      input_visible_rect.width() % subsampling.x != 0 ||
      input_visible_rect.y() % subsampling.y != 0 ||
      input_visible_rect.height() % subsampling.y != 0) {
    return {EncoderStatus::Codes::kInvalidInputFrame,
            base::StrCat({"Input frame visible rectangle ",
                          input_visible_rect.ToString(),
                          " is not properly aligned for the subsampled input "
                          "format"})};
  }
  const gfx::Rect encoder_input_rect(
      0, 0, base::checked_cast<int>(input_size_.Width),
      base::checked_cast<int>(input_size_.Height));

  // The video processor pass below both crops to |input_visible_rect| and, if
  // that region is not already the encoder's input size, scales it to
  // |encoder_input_rect|. Config::input_visible_size is documented as the size
  // clients report via VideoFrame::visible_rect(), so a differing crop is
  // strictly out of contract, but scaling it is deliberate: MF VEA tolerates
  // the same mismatch via PerformD3DScaling(), and rejecting it here would
  // break clients that work on the Media Foundation path.
  if (input_visible_rect != encoder_input_rect ||
      input_frame_desc.Width != input_size_.Width ||
      input_frame_desc.Height != input_size_.Height ||
      input_frame_desc.Format != input_format_ ||
      input_frame_color_space != output_color_space) {
    if (!processed_input_frame_) {
      // The actual input DXGI format and color space are only known here, at
      // Encode() time (e.g. an RGB(A) HDR shared image that must be converted
      // to P010). Validate that the video processor can perform this conversion
      // before allocating resources or submitting any GPU work, so we fail with
      // a clear error rather than deep inside the video processor.
      if (!video_processor_wrapper_->CheckVideoProcessorSupport(
              static_cast<UINT>(input_visible_rect.width()),
              static_cast<UINT>(input_visible_rect.height()),
              input_frame_desc.Format, input_frame_color_space, input_format_,
              output_color_space)) {
        return {EncoderStatus::Codes::kEncoderUnsupportedConfig,
                "D3D12 video processor does not support the required input "
                "format conversion"};
      }
      D3D12_RESOURCE_DESC processed_input_frame_desc =
          CD3DX12_RESOURCE_DESC::Tex2D(input_format_, input_size_.Width,
                                       input_size_.Height, 1, 1);
      HRESULT hr = device_->CreateCommittedResource(
          &D3D12HeapProperties::kDefault, D3D12_HEAP_FLAG_NONE,
          &processed_input_frame_desc, D3D12_RESOURCE_STATE_COMMON, nullptr,
          IID_PPV_ARGS(&processed_input_frame_));
      RETURN_ON_HR_FAILURE(
          hr, "CreateCommittedResource for processed input frame failed",
          EncoderStatus::Codes::kD3D12CreateCommittedResourceFailed);
    }
    if (picture_buffer.fence_and_value.first) {
      if (!video_processor_wrapper_->Wait(picture_buffer.fence_and_value)) {
        return {EncoderStatus::Codes::kD3D12FenceWaitFailed,
                "D3D12 video processor wait failed"};
      }
    }
    auto fence_or_value = video_processor_wrapper_->ProcessFrames(
        picture_buffer.resource.Get(), picture_buffer.subresource,
        input_frame_color_space, input_visible_rect,
        processed_input_frame_.Get(), 0, output_color_space,
        encoder_input_rect);
    if (!fence_or_value.first) {
      return {EncoderStatus::Codes::kD3D12VideoProcessorProcessFramesFailed,
              "D3D12 video processor process frame failed"};
    }

    picture_buffer = {processed_input_frame_, 0, fence_or_value};
  }
  if (picture_buffer.fence_and_value.first) {
    if (!video_encoder_wrapper_->Wait(picture_buffer.fence_and_value)) {
      return {EncoderStatus::Codes::kD3D12FenceWaitFailed,
              "D3D12 video encoder wait failed"};
    }
  }
  auto impl_result =
      EncodeImpl(picture_buffer.resource.Get(), picture_buffer.subresource,
                 options, output_color_space, input_frame_hdr_metadata);
  if (!impl_result.is_ok()) {
    // EncodeImpl() may bail (e.g. kBadReferenceBuffer) after
    // D3D12VideoProcessorWrapper::ProcessFrames() has already submitted work
    // to the video processor queue but before D3D12VideoEncoderWrapper::Encode
    // would have CPU-synced it. Sync now so resources referenced by that
    // command list are not released while the GPU is still using them when
    // teardown follows this error.
    if (auto status = video_processor_wrapper_->WaitForInFlightWork();
        !status.is_ok()) {
      DLOG(ERROR) << "Waiting for in-flight video processing after encode "
                     "error failed: "
                  << static_cast<int>(status.code());
    }
    return std::move(impl_result);
  }

  const base::UnsafeSharedMemoryRegion& region = bitstream_buffer.region();
  CHECK(region.IsValid());
  base::WritableSharedMemoryMapping map = region.Map();
  auto payload_size_or_error =
      ReadbackBitstream(map.GetMemoryAsSpan<uint8_t>());
  if (!payload_size_or_error.has_value()) {
    return std::move(payload_size_or_error).error();
  }
  metadata_.encoded_color_space = output_color_space;
  metadata_.payload_size_bytes = std::move(payload_size_or_error).value();

  EncodeResult encode_result{
      .bitstream_buffer_id = bitstream_buffer.id(),
      .metadata = metadata_,
  };
  return encode_result;
}

uint8_t D3D12VideoEncodeDelegate::GetNumTemporalLayers() const {
  return svc_layers_.has_value() ? svc_layers_->config().num_temporal_layers
                                 : 1;
}

D3D12VideoEncodeDelegate::D3D12VideoEncoderRateControl::
    D3D12VideoEncoderRateControl() = default;

D3D12VideoEncodeDelegate::D3D12VideoEncoderRateControl::
    D3D12VideoEncoderRateControl(const D3D12VideoEncoderRateControl& other)
    : rate_control_(other.rate_control_), params_(other.params_) {
  switch (rate_control_.Mode) {
    case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CQP:
      rate_control_.ConfigParams.pConfiguration_CQP = &params_.cqp;
      break;
    case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CBR:
      rate_control_.ConfigParams.pConfiguration_CBR = &params_.cbr;
      break;
    case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_VBR:
      rate_control_.ConfigParams.pConfiguration_VBR = &params_.vbr;
      break;
    default:
      NOTREACHED();
  }
}

D3D12VideoEncodeDelegate::D3D12VideoEncoderRateControl&
D3D12VideoEncodeDelegate::D3D12VideoEncoderRateControl::operator=(
    const D3D12VideoEncoderRateControl& other) {
  rate_control_ = other.rate_control_;
  params_ = other.params_;
  switch (rate_control_.Mode) {
    case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CQP:
      rate_control_.ConfigParams.pConfiguration_CQP = &params_.cqp;
      break;
    case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CBR:
      rate_control_.ConfigParams.pConfiguration_CBR = &params_.cbr;
      break;
    case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_VBR:
      rate_control_.ConfigParams.pConfiguration_VBR = &params_.vbr;
      break;
    default:
      NOTREACHED();
  }
  return *this;
}

// static
D3D12VideoEncodeDelegate::D3D12VideoEncoderRateControl
D3D12VideoEncodeDelegate::D3D12VideoEncoderRateControl::CreateCqp(
    uint32_t i_frame_qp,
    uint32_t p_frame_qp,
    uint32_t b_frame_qp) {
  D3D12VideoEncoderRateControl rate_control;
  rate_control.params_.cqp = {
      .ConstantQP_FullIntracodedFrame = i_frame_qp,
      .ConstantQP_InterPredictedFrame_PrevRefOnly = p_frame_qp,
      .ConstantQP_InterPredictedFrame_BiDirectionalRef = b_frame_qp};
  rate_control.rate_control_ = {
      .Mode = D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CQP,
      .ConfigParams = {.DataSize = sizeof(rate_control.params_.cqp),
                       .pConfiguration_CQP = &rate_control.params_.cqp},
      .TargetFrameRate = {30, 1},
  };
  return rate_control;
}

// static
D3D12VideoEncodeDelegate::D3D12VideoEncoderRateControl
D3D12VideoEncodeDelegate::D3D12VideoEncoderRateControl::Create(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate,
    ID3D12VideoDevice3* video_device,
    VideoCodecProfile output_profile) {
  D3D12VideoEncoderRateControl rate_control;
  switch (bitrate_allocation.GetMode()) {
    case Bitrate::Mode::kConstant:
      rate_control.params_.cbr = {
          .TargetBitRate = bitrate_allocation.GetSumBps(),
      };
      rate_control.rate_control_ = {
          .Mode = D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CBR,
          .ConfigParams = {.DataSize = sizeof(rate_control.params_.cbr),
                           .pConfiguration_CBR = &rate_control.params_.cbr},
          .TargetFrameRate = {framerate, 1},
      };
      break;
    case Bitrate::Mode::kVariable:
      if (!IsVBRSupported(video_device, output_profile)) {
        LOG(ERROR) << "Requested VBR not supported, falling back to CBR.";
        rate_control.params_.cbr = {
            .TargetBitRate = bitrate_allocation.GetSumBps(),
        };
        rate_control.rate_control_ = {
            .Mode = D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CBR,
            .ConfigParams = {.DataSize = sizeof(rate_control.params_.cbr),
                             .pConfiguration_CBR = &rate_control.params_.cbr},
            .TargetFrameRate = {framerate, 1},
        };
      } else {
        rate_control.params_.vbr = {
            .TargetAvgBitRate = bitrate_allocation.GetSumBps(),
            .PeakBitRate = bitrate_allocation.GetPeakBps(),
        };
        rate_control.rate_control_ = {
            .Mode = D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_VBR,
            .ConfigParams = {.DataSize = sizeof(rate_control.params_.vbr),
                             .pConfiguration_VBR = &rate_control.params_.vbr},
            .TargetFrameRate = {framerate, 1},
        };
      }
      break;
    case Bitrate::Mode::kExternal:
      // The effective QP value will be set before each frame. Filling a
      // commonly used default value that would be most likely supported by the
      // hardware.
      constexpr uint32_t kDefaultQp = 26;
      rate_control.params_.cqp = {
          .ConstantQP_FullIntracodedFrame = kDefaultQp,
          .ConstantQP_InterPredictedFrame_PrevRefOnly = kDefaultQp,
          .ConstantQP_InterPredictedFrame_BiDirectionalRef = kDefaultQp,
      };
      rate_control.rate_control_ = {
          .Mode = D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CQP,
          .ConfigParams = {.DataSize = sizeof(rate_control.params_.cqp),
                           .pConfiguration_CQP = &rate_control.params_.cqp},
          .TargetFrameRate = {framerate, 1},
      };
      break;
  }
  return rate_control;
}

D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE
D3D12VideoEncodeDelegate::D3D12VideoEncoderRateControl::GetMode() const {
  return rate_control_.Mode;
}

void D3D12VideoEncodeDelegate::D3D12VideoEncoderRateControl::SetCQP(
    FrameType frame_type,
    uint32_t qp) {
  CHECK_EQ(rate_control_.Mode, D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CQP);
  switch (frame_type) {
    case FrameType::kIntra:
      params_.cqp.ConstantQP_FullIntracodedFrame = qp;
      break;
    case FrameType::kInterPrev:
      params_.cqp.ConstantQP_InterPredictedFrame_PrevRefOnly = qp;
      break;
    case FrameType::kInterBiDirectional:
      params_.cqp.ConstantQP_InterPredictedFrame_BiDirectionalRef = qp;
      break;
  }
}

bool D3D12VideoEncodeDelegate::D3D12VideoEncoderRateControl::operator==(
    const D3D12VideoEncoderRateControl& other) const {
  CHECK_EQ(rate_control_.TargetFrameRate.Denominator, 1u);
  CHECK_EQ(other.rate_control_.TargetFrameRate.Denominator, 1u);
  if (rate_control_.Mode != other.rate_control_.Mode ||
      rate_control_.Flags != other.rate_control_.Flags ||
      rate_control_.TargetFrameRate.Numerator !=
          other.rate_control_.TargetFrameRate.Numerator) {
    return false;
  }
  switch (rate_control_.Mode) {
    case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CQP:
      return params_.cqp.ConstantQP_FullIntracodedFrame ==
                 other.params_.cqp.ConstantQP_FullIntracodedFrame &&
             params_.cqp.ConstantQP_InterPredictedFrame_PrevRefOnly ==
                 other.params_.cqp.ConstantQP_InterPredictedFrame_PrevRefOnly &&
             params_.cqp.ConstantQP_InterPredictedFrame_BiDirectionalRef ==
                 other.params_.cqp
                     .ConstantQP_InterPredictedFrame_BiDirectionalRef;
    case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CBR:
      CHECK_EQ(rate_control_.Flags, 0u);
      return params_.cbr.TargetBitRate == other.params_.cbr.TargetBitRate;
    case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_VBR:
      CHECK_EQ(rate_control_.Flags, 0u);
      return params_.vbr.TargetAvgBitRate ==
                 other.params_.vbr.TargetAvgBitRate &&
             params_.vbr.PeakBitRate == other.params_.vbr.PeakBitRate;
    default:
      NOTREACHED();
  }
}

EncoderStatus::Or<size_t>
D3D12VideoEncodeDelegate::GetEncodedBitstreamWrittenBytesCount(
    const ScopedD3D12ResourceMap& metadata) {
  if (metadata.data().size() < sizeof(D3D12_VIDEO_ENCODER_OUTPUT_METADATA)) {
    return EncoderStatus::Codes::kEncoderHardwareDriverError;
  }
  return reinterpret_cast<const D3D12_VIDEO_ENCODER_OUTPUT_METADATA*>(
             metadata.data().data())
      ->EncodedBitstreamWrittenBytesCount;
}

EncoderStatus::Or<size_t> D3D12VideoEncodeDelegate::ReadbackBitstream(
    base::span<uint8_t> bitstream_buffer) {
  auto metadata_or_error = video_encoder_wrapper_->GetEncoderOutputMetadata();
  if (!metadata_or_error.has_value()) {
    return std::move(metadata_or_error).error();
  }
  ScopedD3D12ResourceMap metadata = std::move(metadata_or_error).value();
  auto size_or_error = GetEncodedBitstreamWrittenBytesCount(metadata);
  if (!size_or_error.has_value()) {
    return std::move(size_or_error).error();
  }
  size_t size = std::move(size_or_error).value();
  if (size > bitstream_buffer.size()) {
    return {EncoderStatus::Codes::kEncoderHardwareDriverError,
            "Encoded bitstream exceeds output buffer size"};
  }
  D3D12_RANGE written_range{};
  metadata.Commit(&written_range);
  EncoderStatus status =
      video_encoder_wrapper_->ReadbackBitstream(bitstream_buffer.first(size));
  if (!status.is_ok()) {
    return status;
  }
  return size;
}

template <size_t maxDpbSize>
D3D12VideoEncodeDecodedPictureBuffers<
    maxDpbSize>::D3D12VideoEncodeDecodedPictureBuffers() = default;

template <size_t maxDpbSize>
D3D12VideoEncodeDecodedPictureBuffers<
    maxDpbSize>::~D3D12VideoEncodeDecodedPictureBuffers() = default;

template <size_t maxDpbSize>
bool D3D12VideoEncodeDecodedPictureBuffers<
    maxDpbSize>::InitializeTextureResources(ID3D12Device* device,
                                            gfx::Size texture_size,
                                            DXGI_FORMAT format,
                                            size_t max_num_ref_frames,
                                            bool use_texture_array) {
  if (max_num_ref_frames == 0 || max_num_ref_frames > kMaxDpbSize) {
    LOG(ERROR) << "Invalid max reference frames number: " << max_num_ref_frames
               << " (should be between 1 and " << kMaxDpbSize << ")";
    return false;
  }
  size_ = max_num_ref_frames;

  // We reserve one space in extra for the current frame.
  const size_t array_size = size_ + 1;
  resources_.resize(use_texture_array ? 1 : array_size);
  raw_resources_.resize(array_size);
  subresources_.resize(array_size);

  D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(
      format, texture_size.width(), texture_size.height(),
      /*arraySize=*/use_texture_array ? array_size : 1, /*mipLevels=*/1);
  desc.Flags = D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE |
               D3D12_RESOURCE_FLAG_VIDEO_ENCODE_REFERENCE_ONLY;
  if (use_texture_array) {
    HRESULT hr = device->CreateCommittedResource(
        &D3D12HeapProperties::kDefault, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&resources_[0]));
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to CreateCommittedResource for "
                    "D3D12VideoEncodeReferenceFrameList: "
                 << PrintHr(hr);
      return false;
    }
    for (size_t i = 0; i < array_size; i++) {
      raw_resources_[i] = resources_[0].Get();
      // When texture array is used, this points to the array index of the first
      // resource plane. Refer to:
      // https://microsoft.github.io/DirectX-Specs/d3d/D3D12VideoEncoding.html#6112-struct-d3d12_video_encode_reference_frames
      subresources_[i] = i;
    }
  } else {
    for (size_t i = 0; i < array_size; i++) {
      HRESULT hr = device->CreateCommittedResource(
          &D3D12HeapProperties::kDefault, D3D12_HEAP_FLAG_NONE, &desc,
          D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&resources_[i]));
      if (FAILED(hr)) {
        LOG(ERROR) << "Failed to CreateCommittedResource for "
                      "D3D12VideoEncodeReferenceFrameList: "
                   << PrintHr(hr);
        return false;
      }
      raw_resources_[i] = resources_[i].Get();
      subresources_[i] = 0;
    }
  }
  return true;
}

template <size_t maxDpbSize>
D3D12_VIDEO_ENCODER_RECONSTRUCTED_PICTURE
D3D12VideoEncodeDecodedPictureBuffers<maxDpbSize>::GetCurrentFrame() const {
  // Make sure we have initialized.
  CHECK_GT(resources_.size(), 0u);
  // The current frame is at the end of the array to make it convenient for
  // std::ranges::rotate() operation.
  return {raw_resources_.back(), subresources_.back()};
}

template <size_t maxDpbSize>
void D3D12VideoEncodeDecodedPictureBuffers<maxDpbSize>::InsertCurrentFrame(
    size_t position) {
  CHECK_GT(resources_.size(), 0u);
  base::span raw_resources_span = base::span(raw_resources_).subspan(position);
  std::ranges::rotate(raw_resources_span, std::prev(raw_resources_span.end()));
  base::span subresources_span = base::span(subresources_).subspan(position);
  std::ranges::rotate(subresources_span, std::prev(subresources_span.end()));
}

template <size_t maxDpbSize>
void D3D12VideoEncodeDecodedPictureBuffers<maxDpbSize>::ReplaceWithCurrentFrame(
    size_t position) {
  CHECK_GT(resources_.size(), 0u);
  std::swap(raw_resources_[position], raw_resources_.back());
  std::swap(subresources_[position], subresources_.back());
}

template <size_t maxDpbSize>
void D3D12VideoEncodeDecodedPictureBuffers<maxDpbSize>::EraseFrame(
    size_t position) {
  CHECK_LT(position, size());
  base::span raw_resources_span =
      base::span(raw_resources_).first(size()).subspan(position);
  std::ranges::rotate(raw_resources_span,
                      std::next(raw_resources_span.begin()));
  base::span subresources_span =
      base::span(subresources_).first(size()).subspan(position);
  std::ranges::rotate(subresources_span, std::next(subresources_span.begin()));
}

template <size_t maxDpbSize>
D3D12_VIDEO_ENCODE_REFERENCE_FRAMES D3D12VideoEncodeDecodedPictureBuffers<
    maxDpbSize>::ToD3D12VideoEncodeReferenceFrames() {
  return {
      .NumTexture2Ds = static_cast<UINT>(size_),
      .ppTexture2Ds = raw_resources_.data(),
      .pSubresources = subresources_.data(),
  };
}

template class D3D12VideoEncodeDecodedPictureBuffers<H264DPB::kDPBMaxSize>;
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
static_assert(static_cast<size_t>(H264DPB::kDPBMaxSize) ==
              static_cast<size_t>(/*H265*/ kMaxDpbSize));
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

template class D3D12VideoEncodeDecodedPictureBuffers<
    D3D12VideoEncodeDelegate::kAV1DPBMaxSize>;

}  // namespace media
