// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_video_encode_delegate.h"

#include <ranges>

#include "base/bits.h"
#include "base/logging.h"
#include "media/base/win/mf_helpers.h"
#include "media/gpu/h264_dpb.h"
#include "media/gpu/windows/d3d12_helpers.h"
#include "media/gpu/windows/d3d12_video_encode_av1_delegate.h"
#include "media/gpu/windows/d3d12_video_encode_h264_delegate.h"
#include "media/gpu/windows/d3d12_video_encoder_wrapper.h"
#include "third_party/microsoft_dxheaders/src/include/directx/d3dx12_core.h"

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

// static
VideoEncodeAccelerator::SupportedProfiles
D3D12VideoEncodeDelegate::GetSupportedProfiles(
    ID3D12VideoDevice3* video_device) {
  CHECK(video_device);
  VideoEncodeAccelerator::SupportedProfiles supported_profiles;
  for (D3D12_VIDEO_ENCODER_CODEC codec : {
           D3D12_VIDEO_ENCODER_CODEC_H264,
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
           D3D12_VIDEO_ENCODER_CODEC_HEVC,
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
           D3D12_VIDEO_ENCODER_CODEC_AV1,
       }) {
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
    D3D12_FEATURE_DATA_VIDEO_ENCODER_RATE_CONTROL_MODE vbr{
        .Codec = codec,
        .RateControlMode = D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_VBR,
    };
    CHECK_FEATURE_SUPPORT(RATE_CONTROL_MODE, vbr);
    supported_profile.rate_control_modes =
        (cbr.IsSupported ? VideoEncodeAccelerator::kConstantMode
                         : VideoEncodeAccelerator::kNoMode) |
        (vbr.IsSupported ? VideoEncodeAccelerator::kVariableMode
                         : VideoEncodeAccelerator::kNoMode);
    // TODO(crbug.com/40275246): support L1T2/L1T3.
    supported_profile.scalability_modes.push_back(SVCScalabilityMode::kL1T1);
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
        profiles =
            D3D12VideoEncodeAV1Delegate::GetSupportedProfiles(video_device);
        break;
      default:
        NOTREACHED();
    }
    for (const auto& [profile, formats] : profiles) {
      supported_profile.profile = profile;
      supported_profile.gpu_supported_pixel_formats = formats;
      supported_profiles.push_back(supported_profile);
    }
  }
  return supported_profiles;
}

D3D12VideoEncodeDelegate::D3D12VideoEncodeDelegate(
    Microsoft::WRL::ComPtr<ID3D12VideoDevice3> video_device)
    : video_device_(std::move(video_device)) {
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

  CHECK(!config.HasSpatialLayer() && !config.HasTemporalLayer())
      << "D3D12VideoEncoder only support L1T1 mode.";
  max_num_ref_frames_ = 1;

  input_size_.Width = config.input_visible_size.width();
  input_size_.Height = config.input_visible_size.height();

  if ((output_profile_ == H264PROFILE_HIGH10PROFILE ||
       output_profile_ == HEVCPROFILE_MAIN10) &&
      config.input_format == PIXEL_FORMAT_P010LE) {
    input_format_ = DXGI_FORMAT_P010;
  } else {
    input_format_ = DXGI_FORMAT_NV12;
  }
  processed_input_frame_.Reset();

  rate_control_ =
      D3D12VideoEncoderRateControl::Create(config.bitrate, config.framerate);

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

bool D3D12VideoEncodeDelegate::UpdateRateControl(const Bitrate& bitrate,
                                                 uint32_t framerate) {
  auto rate_control = D3D12VideoEncoderRateControl::Create(bitrate, framerate);

  if (rate_control.GetMode() != rate_control_.GetMode() &&
      !SupportsRateControlReconfiguration()) {
    return false;
  }

  rate_control_ = rate_control;
  return true;
}

EncoderStatus::Or<D3D12VideoEncodeDelegate::EncodeResult>
D3D12VideoEncodeDelegate::Encode(
    Microsoft::WRL::ComPtr<ID3D12Resource> input_frame,
    UINT input_frame_subresource,
    const gfx::ColorSpace& input_frame_color_space,
    const BitstreamBuffer& bitstream_buffer,
    const VideoEncoder::EncodeOptions& options) {
  if (D3D12_RESOURCE_DESC input_frame_desc = input_frame->GetDesc();
      input_frame_desc.Width != input_size_.Width ||
      input_frame_desc.Height != input_size_.Height ||
      input_frame_desc.Format != input_format_) {
    if (!processed_input_frame_) {
      D3D12_RESOURCE_DESC processed_input_frame_desc =
          CD3DX12_RESOURCE_DESC::Tex2D(input_format_, input_size_.Width,
                                       input_size_.Height, 1, 1);
      HRESULT hr = device_->CreateCommittedResource(
          &D3D12HeapProperties::kDefault, D3D12_HEAP_FLAG_NONE,
          &processed_input_frame_desc, D3D12_RESOURCE_STATE_COMMON, nullptr,
          IID_PPV_ARGS(&processed_input_frame_));
      RETURN_ON_HR_FAILURE(
          hr, "CreateCommittedResource for processed input frame failed",
          EncoderStatus::Codes::kSystemAPICallError);
    }
    bool ok = video_processor_wrapper_->ProcessFrames(
        input_frame.Get(), input_frame_subresource, input_frame_color_space,
        gfx::Rect(0, 0, input_frame_desc.Width, input_frame_desc.Height),
        processed_input_frame_.Get(), 0, input_frame_color_space,
        gfx::Rect(0, 0, input_size_.Width, input_size_.Height));
    if (!ok) {
      return {EncoderStatus::Codes::kSystemAPICallError,
              "D3D12 video processor process frame failed"};
    }

    input_frame = processed_input_frame_;
    input_frame_subresource = 0;
  }

  auto impl_result =
      EncodeImpl(input_frame.Get(), input_frame_subresource, options);
  if (!impl_result.has_value()) {
    return std::move(impl_result).error();
  }

  const base::UnsafeSharedMemoryRegion& region = bitstream_buffer.region();
  CHECK(region.IsValid());
  base::WritableSharedMemoryMapping map = region.Map();
  auto payload_size_or_error =
      ReadbackBitstream(map.GetMemoryAsSpan<uint8_t>());
  if (!payload_size_or_error.has_value()) {
    return std::move(payload_size_or_error).error();
  }
  EncodeResult encode_result{
      .bitstream_buffer_id_ = bitstream_buffer.id(),
      .metadata_ = std::move(impl_result).value(),
  };
  encode_result.metadata_.encoded_color_space = input_frame_color_space;
  encode_result.metadata_.payload_size_bytes =
      std::move(payload_size_or_error).value();
  return encode_result;
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
    Bitrate bitrate,
    uint32_t framerate) {
  D3D12VideoEncoderRateControl rate_control;
  switch (bitrate.mode()) {
    case Bitrate::Mode::kConstant:
      rate_control.params_.cbr = {
          .TargetBitRate = bitrate.target_bps(),
      };
      rate_control.rate_control_ = {
          .Mode = D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CBR,
          .ConfigParams = {.DataSize = sizeof(rate_control.params_.cbr),
                           .pConfiguration_CBR = &rate_control.params_.cbr},
          .TargetFrameRate = {framerate, 1},
      };
      break;
    case Bitrate::Mode::kVariable:
      rate_control.params_.vbr = {
          .TargetAvgBitRate = bitrate.target_bps(),
          .PeakBitRate = bitrate.peak_bps(),
      };
      rate_control.rate_control_ = {
          .Mode = D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_VBR,
          .ConfigParams = {.DataSize = sizeof(rate_control.params_.vbr),
                           .pConfiguration_VBR = &rate_control.params_.vbr},
          .TargetFrameRate = {framerate, 1},
      };
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

EncoderStatus::Or<size_t> D3D12VideoEncodeDelegate::ReadbackBitstream(
    base::span<uint8_t> bitstream_buffer) {
  auto size_or_error =
      video_encoder_wrapper_->GetEncodedBitstreamWrittenBytesCount();
  if (!size_or_error.has_value()) {
    return std::move(size_or_error).error();
  }
  uint32_t size = std::move(size_or_error).value();
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
bool D3D12VideoEncodeDecodedPictureBuffers<maxDpbSize>::InitializeTextureArray(
    ID3D12Device* device,
    gfx::Size texture_size,
    DXGI_FORMAT format,
    size_t max_num_ref_frames) {
  CHECK_GT(max_num_ref_frames, 0u);
  CHECK_LE(max_num_ref_frames, kMaxDpbSize);
  size_ = max_num_ref_frames;

  // We reserve one space in extra for the current frame.
  const size_t array_size = size_ + 1;
  resources_.resize(array_size);
  raw_resources_.resize(array_size);
  subresources_.resize(array_size);
  D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(
      format, texture_size.width(), texture_size.height(), 1, 1);
  desc.Flags = D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE |
               D3D12_RESOURCE_FLAG_VIDEO_ENCODE_REFERENCE_ONLY;
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
  return true;
}

template <size_t maxDpbSize>
D3D12PictureBuffer
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
