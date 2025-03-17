// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_video_encode_h265_delegate.h"

#include <ranges>

#include "base/bits.h"
#include "base/containers/fixed_flat_map.h"
#include "media/gpu/h265_builder.h"
#include "media/gpu/windows/d3d12_video_helpers.h"
#include "media/gpu/windows/format_utils.h"

namespace media {

namespace {

// Annex A.4.1 General tier and level limits
// - general_level_idc and sub_layer_level_idc[ i ] shall be set equal to a
//   value of 30 times the level number specified in Table A.8.
//
// https://github.com/microsoft/DirectX-Specs/blob/master/d3d/D3D12VideoEncoding.md#level_idc-mappings-for-hevc
constexpr auto kD3D12H265LevelToH265LevelIDCMap =
    base::MakeFixedFlatMap<D3D12_VIDEO_ENCODER_LEVELS_HEVC, uint8_t>({
        {D3D12_VIDEO_ENCODER_LEVELS_HEVC_1, 30},
        {D3D12_VIDEO_ENCODER_LEVELS_HEVC_2, 60},
        {D3D12_VIDEO_ENCODER_LEVELS_HEVC_21, 63},
        {D3D12_VIDEO_ENCODER_LEVELS_HEVC_3, 90},
        {D3D12_VIDEO_ENCODER_LEVELS_HEVC_31, 93},
        {D3D12_VIDEO_ENCODER_LEVELS_HEVC_4, 120},
        {D3D12_VIDEO_ENCODER_LEVELS_HEVC_41, 123},
        {D3D12_VIDEO_ENCODER_LEVELS_HEVC_5, 150},
        {D3D12_VIDEO_ENCODER_LEVELS_HEVC_51, 153},
        {D3D12_VIDEO_ENCODER_LEVELS_HEVC_52, 156},
        {D3D12_VIDEO_ENCODER_LEVELS_HEVC_6, 180},
        {D3D12_VIDEO_ENCODER_LEVELS_HEVC_61, 183},
        {D3D12_VIDEO_ENCODER_LEVELS_HEVC_62, 186},
    });

constexpr auto kVideoCodecProfileToD3D12Profile =
    base::MakeFixedFlatMap<VideoCodecProfile, D3D12_VIDEO_ENCODER_PROFILE_HEVC>(
        {
            {HEVCPROFILE_MAIN, D3D12_VIDEO_ENCODER_PROFILE_HEVC_MAIN},
            {HEVCPROFILE_MAIN10, D3D12_VIDEO_ENCODER_PROFILE_HEVC_MAIN10},
        });

uint8_t D3D12VideoEncoderLevelsHevcToH265LevelIDC(
    D3D12_VIDEO_ENCODER_LEVELS_HEVC level) {
  return kD3D12H265LevelToH265LevelIDCMap.at(level);
}

}  // namespace

D3D12VideoEncodeH265ReferenceFrameManager::
    D3D12VideoEncodeH265ReferenceFrameManager(size_t max_num_ref_frames)
    : max_num_ref_frames_(max_num_ref_frames) {
  CHECK_GT(max_num_ref_frames, 0u);
  CHECK_LE(max_num_ref_frames, kMaxDpbSize);
}
D3D12VideoEncodeH265ReferenceFrameManager::
    ~D3D12VideoEncodeH265ReferenceFrameManager() = default;

void D3D12VideoEncodeH265ReferenceFrameManager::EndFrame(
    uint32_t pic_order_count,
    uint32_t temporal_layer_id) {
  if (descriptors_.size() == max_num_ref_frames_) {
    descriptors_.pop_back();
  }
  descriptors_.insert(descriptors_.begin(),
                      {
                          .PictureOrderCountNumber = pic_order_count,
                          .TemporalLayerIndex = temporal_layer_id,
                      });
  for (size_t i = 0; i < descriptors_.size(); i++) {
    descriptors_[i].ReconstructedPictureResourceIndex = i;
  }
}

void D3D12VideoEncodeH265ReferenceFrameManager::
    WriteReferencePictureDescriptorsToPictureParameters(
        D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_HEVC* pic_params,
        base::span<uint32_t> list0_reference_frames) {
  CHECK(pic_params);
  for (auto& descriptor : descriptors_) {
    descriptor.IsRefUsedByCurrentPic = false;
  }
  for (uint32_t reference_frame_id : list0_reference_frames) {
    CHECK_LT(reference_frame_id, descriptors_.size());
    descriptors_[reference_frame_id].IsRefUsedByCurrentPic = true;
  }
  CHECK_EQ(pic_params->List1ReferenceFramesCount, 0u);
  pic_params->ReferenceFramesReconPictureDescriptorsCount = descriptors_.size();
  pic_params->pReferenceFramesReconPictureDescriptors = descriptors_.data();
}

// static
std::vector<std::pair<VideoCodecProfile, std::vector<VideoPixelFormat>>>
D3D12VideoEncodeH265Delegate::GetSupportedProfiles(
    ID3D12VideoDevice3* video_device) {
  CHECK(video_device);
  std::vector<std::pair<VideoCodecProfile, std::vector<VideoPixelFormat>>>
      profiles;
  for (auto [video_codec_profile, h265_profile] :
       kVideoCodecProfileToD3D12Profile) {
    D3D12_VIDEO_ENCODER_LEVEL_TIER_CONSTRAINTS_HEVC min_level;
    D3D12_VIDEO_ENCODER_LEVEL_TIER_CONSTRAINTS_HEVC max_level;
    D3D12_FEATURE_DATA_VIDEO_ENCODER_PROFILE_LEVEL profile_level{
        .Codec = D3D12_VIDEO_ENCODER_CODEC_HEVC,
        .Profile = {.DataSize = sizeof(h265_profile),
                    .pHEVCProfile = &h265_profile},
        .MinSupportedLevel = {.DataSize = sizeof(min_level),
                              .pHEVCLevelSetting = &min_level},
        .MaxSupportedLevel = {.DataSize = sizeof(max_level),
                              .pHEVCLevelSetting = &max_level},
    };
    if (!CheckD3D12VideoEncoderProfileLevel(video_device, &profile_level)
             .is_ok()) {
      continue;
    }
    std::vector<VideoPixelFormat> formats;
    for (VideoPixelFormat format : {PIXEL_FORMAT_NV12, PIXEL_FORMAT_P010LE}) {
      D3D12_FEATURE_DATA_VIDEO_ENCODER_INPUT_FORMAT input_format{
          .Codec = D3D12_VIDEO_ENCODER_CODEC_HEVC,
          .Profile = profile_level.Profile,
          .Format = VideoPixelFormatToDxgiFormat(format),
      };
      if (CheckD3D12VideoEncoderInputFormat(video_device, &input_format)
              .is_ok()) {
        formats.push_back(format);
      }
    }
    if (!formats.empty()) {
      profiles.emplace_back(video_codec_profile, formats);
    }
  }
  return profiles;
}

D3D12VideoEncodeH265Delegate::D3D12VideoEncodeH265Delegate(
    Microsoft::WRL::ComPtr<ID3D12VideoDevice3> video_device)
    : D3D12VideoEncodeDelegate(std::move(video_device)) {
  // We always do add-one before encoding, so we assign them to be -1 to make it
  // start with 0.
  pic_params_.PictureOrderCountNumber = -1;
  input_arguments_.SequenceControlDesc.CodecGopSequence = {
      .DataSize = sizeof(gop_structure_),
      .pHEVCGroupOfPictures = &gop_structure_,
  };
  input_arguments_.PictureControlDesc.PictureControlCodecData = {
      .DataSize = sizeof(pic_params_),
      .pHEVCPicData = &pic_params_,
  };
}

D3D12VideoEncodeH265Delegate::~D3D12VideoEncodeH265Delegate() = default;

size_t D3D12VideoEncodeH265Delegate::GetMaxNumOfRefFrames() const {
  return max_num_ref_frames_;
}

bool D3D12VideoEncodeH265Delegate::SupportsRateControlReconfiguration() const {
  return encoder_support_flags_ &
         D3D12_VIDEO_ENCODER_SUPPORT_FLAG_RATE_CONTROL_RECONFIGURATION_AVAILABLE;
}

EncoderStatus::Or<BitstreamBufferMetadata>
D3D12VideoEncodeH265Delegate::EncodeImpl(ID3D12Resource* input_frame,
                                         UINT input_frame_subresource,
                                         bool force_keyframe) {
  // Filling the |input_arguments_| according to
  // https://github.com/microsoft/DirectX-Specs/blob/master/d3d/D3D12VideoEncoding.md#6120-struct-d3d12_video_encoder_input_arguments

  if (++pic_params_.PictureOrderCountNumber == gop_structure_.GOPLength) {
    pic_params_.PictureOrderCountNumber = 0;
  }
  bool is_keyframe = pic_params_.PictureOrderCountNumber == 0 || force_keyframe;
  if (is_keyframe) {
    H265VPS vps = ToVPS();
    H265SPS sps = ToSPS(vps);
    H265PPS pps = ToPPS(sps);
    packed_header_.Reset();
    BuildPackedH265VPS(packed_header_, vps);
    BuildPackedH265SPS(packed_header_, sps);
    BuildPackedH265PPS(packed_header_, pps);

    input_arguments_.PictureControlDesc.ReferenceFrames = {};
    pic_params_.FrameType = D3D12_VIDEO_ENCODER_FRAME_TYPE_HEVC_IDR_FRAME;
    pic_params_.PictureOrderCountNumber = 0;
    pic_params_.ReferenceFramesReconPictureDescriptorsCount = 0;
    pic_params_.pReferenceFramesReconPictureDescriptors = nullptr;
    pic_params_.List0ReferenceFramesCount = 0;
    pic_params_.pList0ReferenceFrames = nullptr;
  } else {
    input_arguments_.PictureControlDesc.ReferenceFrames =
        dpb_->ToD3D12VideoEncodeReferenceFrames();
    pic_params_.FrameType = D3D12_VIDEO_ENCODER_FRAME_TYPE_HEVC_P_FRAME;
    list0_reference_frames_[0] = 0;
    pic_params_.List0ReferenceFramesCount = 1;
    pic_params_.pList0ReferenceFrames = list0_reference_frames_.data();
    reference_frame_manager_
        ->WriteReferencePictureDescriptorsToPictureParameters(
            &pic_params_, base::span(list0_reference_frames_).first(1u));
  }
  input_arguments_.PictureControlDesc.ReferenceFrames.NumTexture2Ds = std::min(
      input_arguments_.PictureControlDesc.ReferenceFrames.NumTexture2Ds,
      pic_params_.ReferenceFramesReconPictureDescriptorsCount);

  if (rate_control_ != current_rate_control_) {
    if (rate_control_.GetMode() != current_rate_control_.GetMode()) {
      CHECK(SupportsRateControlReconfiguration());
      input_arguments_.SequenceControlDesc.Flags |=
          D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_FLAG_RATE_CONTROL_CHANGE;
    }
    current_rate_control_ = rate_control_;
    input_arguments_.SequenceControlDesc.RateControl =
        current_rate_control_.GetD3D12VideoEncoderRateControl();
  }

  input_arguments_.PictureControlDesc.Flags =
      D3D12_VIDEO_ENCODER_PICTURE_CONTROL_FLAG_USED_AS_REFERENCE_PICTURE;
  input_arguments_.pInputFrame = input_frame;
  input_arguments_.InputFrameSubresource = input_frame_subresource;
  D3D12PictureBuffer reconstructed_picture = dpb_->GetCurrentFrame();
  EncoderStatus result = video_encoder_wrapper_->Encode(
      input_arguments_,
      {
          .pReconstructedPicture = reconstructed_picture.resource_,
          .ReconstructedPictureSubresource = reconstructed_picture.subresource_,
      });
  if (!result.is_ok()) {
    return result;
  }

  dpb_->InsertCurrentFrame(0);
  reference_frame_manager_->EndFrame(pic_params_.PictureOrderCountNumber,
                                     pic_params_.TemporalLayerIndex);

  BitstreamBufferMetadata metadata;
  metadata.key_frame = is_keyframe;
  return metadata;
}

EncoderStatus D3D12VideoEncodeH265Delegate::InitializeVideoEncoder(
    const VideoEncodeAccelerator::Config& config) {
  CHECK_EQ(VideoCodecProfileToVideoCodec(config.output_profile),
           VideoCodec::kHEVC);

  D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC codec{
      .Codec = D3D12_VIDEO_ENCODER_CODEC_HEVC};
  EncoderStatus status =
      CheckD3D12VideoEncoderCodec(video_device_.Get(), &codec);
  if (!status.is_ok()) {
    return status;
  }

  if (!kVideoCodecProfileToD3D12Profile.contains(config.output_profile)) {
    return {
        EncoderStatus::Codes::kEncoderUnsupportedProfile,
        base::StringPrintf(
            "D3D12VideoEncoder only support H265 main/main10 profile, got %s",
            GetProfileName(config.output_profile))};
  }

  h265_profile_ = kVideoCodecProfileToD3D12Profile.at(config.output_profile);
  D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC
  codec_config_support_hevc;
  D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT
  codec_config_support{
      .Codec = D3D12_VIDEO_ENCODER_CODEC_HEVC,
      .Profile = {.DataSize = sizeof(h265_profile_),
                  .pHEVCProfile = &h265_profile_},
      .CodecSupportLimits = {.DataSize = sizeof(codec_config_support_hevc),
                             .pHEVCSupport = &codec_config_support_hevc},
  };
  status = CheckD3D12VideoEncoderCodecConfigurationSupport(
      video_device_.Get(), &codec_config_support);
  if (!status.is_ok()) {
    return status;
  }
  codec_config_hevc_ = {
      .ConfigurationFlags =
          (codec_config_support_hevc.SupportFlags &
                   D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_SAO_FILTER_SUPPORT
               ? D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_FLAG_ENABLE_SAO_FILTER
               : D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_FLAG_NONE) |
          (codec_config_support_hevc.SupportFlags &
                   D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_ASYMETRIC_MOTION_PARTITION_REQUIRED
               ? D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_FLAG_USE_ASYMETRIC_MOTION_PARTITION
               : D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_FLAG_NONE) |
          (codec_config_support_hevc.SupportFlags &
                   D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_TRANSFORM_SKIP_SUPPORT
               ? D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_FLAG_ENABLE_TRANSFORM_SKIPPING
               : D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_FLAG_NONE),
      .MinLumaCodingUnitSize = codec_config_support_hevc.MinLumaCodingUnitSize,
      .MaxLumaCodingUnitSize = codec_config_support_hevc.MaxLumaCodingUnitSize,
      .MinLumaTransformUnitSize =
          codec_config_support_hevc.MinLumaTransformUnitSize,
      .MaxLumaTransformUnitSize =
          codec_config_support_hevc.MaxLumaTransformUnitSize,
      .max_transform_hierarchy_depth_inter =
          codec_config_support_hevc.max_transform_hierarchy_depth_inter,
      .max_transform_hierarchy_depth_intra =
          codec_config_support_hevc.max_transform_hierarchy_depth_intra,
  };

  uint32_t gop_length = config.gop_length.value();
  gop_structure_ = {
      .GOPLength = gop_length,
      .PPicturePeriod = 1,
      .log2_max_pic_order_cnt_lsb_minus4 = 0,
  };

  D3D12_VIDEO_ENCODER_PROFILE_HEVC suggested_profile;
  D3D12_VIDEO_ENCODER_LEVEL_TIER_CONSTRAINTS_HEVC suggested_level;
  D3D12_FEATURE_DATA_VIDEO_ENCODER_SUPPORT support{
      .Codec = D3D12_VIDEO_ENCODER_CODEC_HEVC,
      .InputFormat = input_format_,
      .CodecConfiguration = {.DataSize = sizeof(codec_config_hevc_),
                             .pHEVCConfig = &codec_config_hevc_},
      .CodecGopSequence = {.DataSize = sizeof(gop_structure_),
                           .pHEVCGroupOfPictures = &gop_structure_},
      .RateControl = rate_control_.GetD3D12VideoEncoderRateControl(),
      .IntraRefresh = D3D12_VIDEO_ENCODER_INTRA_REFRESH_MODE_NONE,
      .SubregionFrameEncoding =
          D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_FULL_FRAME,
      .ResolutionsListCount = 1,
      .pResolutionList = &input_size_,
      .MaxReferenceFramesInDPB = static_cast<UINT>(max_num_ref_frames_),
      .SuggestedProfile = {.DataSize = sizeof(suggested_profile),
                           .pHEVCProfile = &suggested_profile},
      .SuggestedLevel = {.DataSize = sizeof(suggested_level),
                         .pHEVCLevelSetting = &suggested_level},
      .pResolutionDependentSupport = &resolution_support_limits_,
  };
  status = CheckD3D12VideoEncoderSupport(video_device_.Get(), &support);
  if (!status.is_ok()) {
    return status;
  }
  encoder_support_flags_ = support.SupportFlags;

  h265_level_ = suggested_level;

  dpb_.emplace(max_num_ref_frames_);
  dpb_->InitializeTextureArray(device_.Get(), config.input_visible_size,
                               input_format_);
  reference_frame_manager_.emplace(max_num_ref_frames_);

  video_encoder_wrapper_ = video_encoder_wrapper_factory_.Run(
      video_device_.Get(), D3D12_VIDEO_ENCODER_CODEC_HEVC,
      {.DataSize = sizeof(h265_profile_), .pHEVCProfile = &h265_profile_},
      {.DataSize = sizeof(h265_level_), .pHEVCLevelSetting = &h265_level_},
      input_format_,
      {.DataSize = sizeof(codec_config_hevc_),
       .pHEVCConfig = &codec_config_hevc_},
      input_size_);
  if (!video_encoder_wrapper_->Initialize()) {
    return EncoderStatus::Codes::kEncoderInitializationError;
  }

  current_rate_control_ = rate_control_;
  input_arguments_.SequenceControlDesc.RateControl =
      current_rate_control_.GetD3D12VideoEncoderRateControl();
  input_arguments_.SequenceControlDesc.PictureTargetResolution = input_size_;
  return EncoderStatus::Codes::kOk;
}

EncoderStatus::Or<size_t> D3D12VideoEncodeH265Delegate::ReadbackBitstream(
    base::span<uint8_t> bitstream_buffer) {
  size_t packed_header_size = packed_header_.BytesInBuffer();
  // The |bitstream_buffer| is from outer shared memory, and the
  // |packed_header_| is created in this class, so they won't overlap.
  bitstream_buffer.first(packed_header_size)
      .copy_from_nonoverlapping(packed_header_.data());
  packed_header_.Reset();
  bitstream_buffer = bitstream_buffer.subspan(packed_header_size);

  // Adding a 0x00 byte to make sure the first NALU of each frame has 0x00000001
  // start code.
  bitstream_buffer[0] = 0x00u;
  bitstream_buffer = bitstream_buffer.subspan(1u);

  auto size_or_error =
      D3D12VideoEncodeDelegate::ReadbackBitstream(bitstream_buffer);
  if (!size_or_error.has_value()) {
    return std::move(size_or_error).error();
  }

  if (bitstream_buffer.first(3u) != base::span_from_cstring("\0\0\1")) {
    return {EncoderStatus::Codes::kBitstreamConversionError,
            "D3D12VideoEncodeH265Delegate: The encoded bitstream does not "
            "start with 0x000001"};
  }
  return packed_header_size + 1 + std::move(size_or_error).value();
}

H265VPS D3D12VideoEncodeH265Delegate::ToVPS() const {
  // HEVC Video Parameter Set
  // https://github.com/microsoft/DirectX-Specs/blob/master/d3d/D3D12VideoEncoding.md#hevc-video-parameter-set-expected-values
  H265VPS vps;
  vps.vps_video_parameter_set_id = 0;
  vps.vps_temporal_id_nesting_flag = true;
  vps.profile_tier_level.general_profile_idc = h265_profile_ + 1;
  vps.profile_tier_level.general_profile_compatibility_flags =
      1u << (31 - vps.profile_tier_level.general_profile_idc);
  vps.profile_tier_level.general_progressive_source_flag = true;
  vps.profile_tier_level.general_non_packed_constraint_flag = true;
  vps.profile_tier_level.general_frame_only_constraint_flag = true;
  vps.profile_tier_level.general_level_idc =
      D3D12VideoEncoderLevelsHevcToH265LevelIDC(h265_level_.Level);
  vps.vps_max_dec_pic_buffering_minus1[0] = max_num_ref_frames_;
  vps.vps_max_latency_increase_plus1[0] = 1;
  return vps;
}

H265SPS D3D12VideoEncodeH265Delegate::ToSPS(const H265VPS& vps) const {
  // HEVC Sequence Parameter Set
  // https://microsoft.github.io/DirectX-Specs/d3d/D3D12VideoEncoding.html#hevc-sequence-parameter-set-expected-values
  H265SPS sps;
  sps.sps_video_parameter_set_id = vps.vps_video_parameter_set_id;
  sps.sps_max_sub_layers_minus1 = vps.vps_max_sub_layers_minus1;
  sps.sps_temporal_id_nesting_flag = vps.vps_temporal_id_nesting_flag;
  sps.profile_tier_level = vps.profile_tier_level;
  sps.sps_seq_parameter_set_id = 0;
  sps.chroma_format_idc = 1;
  sps.pic_width_in_luma_samples = base::bits::AlignUp(
      input_size_.Width, resolution_support_limits_.SubregionBlockPixelsSize);
  sps.pic_height_in_luma_samples = base::bits::AlignUp(
      input_size_.Height, resolution_support_limits_.SubregionBlockPixelsSize);
  sps.conf_win_right_offset =
      (sps.pic_width_in_luma_samples - input_size_.Width) >> 1;
  sps.conf_win_bottom_offset =
      (sps.pic_height_in_luma_samples - input_size_.Height) >> 1;
  sps.log2_max_pic_order_cnt_lsb_minus4 =
      gop_structure_.log2_max_pic_order_cnt_lsb_minus4;
  std::ranges::copy(vps.vps_max_dec_pic_buffering_minus1,
                    sps.sps_max_dec_pic_buffering_minus1);
  std::ranges::copy(vps.vps_max_num_reorder_pics, sps.sps_max_num_reorder_pics);
  std::ranges::copy(vps.vps_max_latency_increase_plus1,
                    sps.sps_max_latency_increase_plus1);
  sps.log2_min_luma_coding_block_size_minus3 =
      codec_config_hevc_.MinLumaCodingUnitSize;
  sps.log2_diff_max_min_luma_coding_block_size =
      codec_config_hevc_.MaxLumaCodingUnitSize -
      codec_config_hevc_.MinLumaCodingUnitSize;
  sps.log2_min_luma_transform_block_size_minus2 =
      codec_config_hevc_.MinLumaTransformUnitSize;
  sps.log2_diff_max_min_luma_transform_block_size =
      codec_config_hevc_.MaxLumaTransformUnitSize -
      codec_config_hevc_.MinLumaTransformUnitSize;
  sps.max_transform_hierarchy_depth_inter =
      codec_config_hevc_.max_transform_hierarchy_depth_inter;
  sps.max_transform_hierarchy_depth_intra =
      codec_config_hevc_.max_transform_hierarchy_depth_intra;
  sps.amp_enabled_flag =
      codec_config_hevc_.ConfigurationFlags &
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_FLAG_USE_ASYMETRIC_MOTION_PARTITION;
  sps.sample_adaptive_offset_enabled_flag =
      codec_config_hevc_.ConfigurationFlags &
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_FLAG_ENABLE_SAO_FILTER;
  sps.long_term_ref_pics_present_flag =
      codec_config_hevc_.ConfigurationFlags &
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_FLAG_ENABLE_LONG_TERM_REFERENCES;
  return sps;
}

H265PPS D3D12VideoEncodeH265Delegate::ToPPS(const H265SPS& sps) const {
  // HEVC Picture Parameter Set
  // https://microsoft.github.io/DirectX-Specs/d3d/D3D12VideoEncoding.html#hevc-picture-parameter-set-expected-values
  H265PPS pps;
  pps.pps_pic_parameter_set_id = 0;
  pps.pps_seq_parameter_set_id = sps.sps_seq_parameter_set_id;
  pps.cabac_init_present_flag = true;
  pps.constrained_intra_pred_flag =
      codec_config_hevc_.ConfigurationFlags &
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_FLAG_USE_CONSTRAINED_INTRAPREDICTION;
  pps.transform_skip_enabled_flag =
      codec_config_hevc_.ConfigurationFlags &
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_FLAG_ENABLE_TRANSFORM_SKIPPING;
  pps.cu_qp_delta_enabled_flag = true;
  pps.pps_slice_chroma_qp_offsets_present_flag = true;
  pps.pps_loop_filter_across_slices_enabled_flag = !(
      codec_config_hevc_.ConfigurationFlags &
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_FLAG_DISABLE_LOOP_FILTER_ACROSS_SLICES);
  pps.deblocking_filter_control_present_flag = true;
  return pps;
}

}  // namespace media
