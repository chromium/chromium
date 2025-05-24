// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_video_encode_av1_delegate.h"

#include "base/containers/fixed_flat_map.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "media/gpu/gpu_video_encode_accelerator_helpers.h"
#include "media/gpu/windows/d3d12_video_helpers.h"
#include "media/gpu/windows/format_utils.h"
#include "third_party/libaom/source/libaom/av1/ratectrl_rtc.h"

namespace media {

namespace {

constexpr uint32_t kDefaultOrderHintBitsMinus1 = 7;
constexpr uint32_t kPrimaryRefNone = 7;

// Default value from
// //third_party/webrtc/modules/video_coding/codecs/av1/libaom_av1_encoder.cc,
constexpr uint8_t kAV1MinQuantizer = 10;
// //third_party/webrtc/media/engine/webrtc_video_engine.h.
constexpr uint8_t kAV1MaxQuantizer = 56;

// Sensible default values for CDEF taken from
// https://github.com/intel/libva-utils/blob/master/encode/av1encode.c
constexpr std::array<uint8_t, 8> kCdefYPriStrength = {9, 12, 0, 6, 2, 4, 1, 2};
constexpr std::array<uint8_t, 8> KCdefYSecStrength = {0, 2, 0, 0, 0, 1, 0, 1};
constexpr std::array<uint8_t, 8> kCdefUVPriStrength = {9, 12, 0, 6, 2, 4, 1, 2};
constexpr std::array<uint8_t, 8> kCdefUvSecStrength = {0, 2, 0, 0, 0, 1, 0, 1};

constexpr auto kVideoCodecProfileToD3D12Profile =
    base::MakeFixedFlatMap<VideoCodecProfile, D3D12_VIDEO_ENCODER_AV1_PROFILE>(
        {{AV1PROFILE_PROFILE_MAIN, D3D12_VIDEO_ENCODER_AV1_PROFILE_MAIN},
         {AV1PROFILE_PROFILE_HIGH, D3D12_VIDEO_ENCODER_AV1_PROFILE_HIGH},
         {AV1PROFILE_PROFILE_PRO,
          D3D12_VIDEO_ENCODER_AV1_PROFILE_PROFESSIONAL}});

AV1BitstreamBuilder::SequenceHeader FillAV1BuilderSequenceHeader(
    D3D12_VIDEO_ENCODER_AV1_PROFILE profile,
    const D3D12_VIDEO_ENCODER_PICTURE_RESOLUTION_DESC& input_size,
    const D3D12_VIDEO_ENCODER_AV1_LEVEL_TIER_CONSTRAINTS& tier_level) {
  AV1BitstreamBuilder::SequenceHeader sequence_header{};

  sequence_header.profile = profile;
  sequence_header.level[0] = tier_level.Level;
  sequence_header.tier[0] = tier_level.Tier;
  sequence_header.operating_points_cnt_minus_1 = 0;
  sequence_header.frame_width_bits_minus_1 = 15;
  sequence_header.frame_height_bits_minus_1 = 15;
  sequence_header.width = input_size.Width;
  sequence_header.height = input_size.Height;
  sequence_header.order_hint_bits_minus_1 = kDefaultOrderHintBitsMinus1;

  sequence_header.use_128x128_superblock = false;
  sequence_header.enable_filter_intra = false;
  sequence_header.enable_intra_edge_filter = false;
  sequence_header.enable_interintra_compound = false;
  sequence_header.enable_masked_compound = false;
  sequence_header.enable_warped_motion = false;
  sequence_header.enable_dual_filter = false;
  sequence_header.enable_order_hint = true;
  sequence_header.enable_jnt_comp = false;
  sequence_header.enable_ref_frame_mvs = false;
  sequence_header.enable_superres = false;
  sequence_header.enable_cdef = true;
  sequence_header.enable_restoration = false;

  return sequence_header;
}

AV1BitstreamBuilder::FrameHeader FillAV1BuilderFrameHeader(
    const D3D12VideoEncodeAV1Delegate::PictureControlFlags& picture_ctrl,
    const D3D12_VIDEO_ENCODER_AV1_PICTURE_CONTROL_CODEC_DATA& pic_params) {
  AV1BitstreamBuilder::FrameHeader frame_header{};
  frame_header.frame_type =
      pic_params.FrameType == D3D12_VIDEO_ENCODER_AV1_FRAME_TYPE_KEY_FRAME
          ? libgav1::FrameType::kFrameKey
          : libgav1::FrameType::kFrameInter;
  frame_header.error_resilient_mode = false;
  frame_header.disable_cdf_update = false;
  frame_header.disable_frame_end_update_cdf = false;
  frame_header.base_qindex = pic_params.Quantization.BaseQIndex;
  frame_header.order_hint = pic_params.OrderHint;
  frame_header.filter_level[0] =
      base::span(pic_params.LoopFilter.LoopFilterLevel)[0];
  frame_header.filter_level[1] =
      base::span(pic_params.LoopFilter.LoopFilterLevel)[1];
  frame_header.filter_level_u = pic_params.LoopFilter.LoopFilterLevelU;
  frame_header.filter_level_v = pic_params.LoopFilter.LoopFilterLevelV;
  frame_header.sharpness_level = 0;
  frame_header.loop_filter_delta_enabled = false;
  frame_header.primary_ref_frame = pic_params.PrimaryRefFrame;

  for (uint32_t i = 0; i < std::size(pic_params.ReferenceIndices); i++) {
    frame_header.ref_frame_idx[i] = base::span(pic_params.ReferenceIndices)[i];
  }
  frame_header.refresh_frame_flags = pic_params.RefreshFrameFlags;
  base::span descriptors = pic_params.ReferenceFramesReconPictureDescriptors;
  for (uint32_t i = 0; i < std::size(descriptors); i++) {
    frame_header.ref_order_hint[i] = descriptors[i].OrderHint;
  }

  const auto& cdef = pic_params.CDEF;
  CHECK_LE(1u << cdef.CdefBits, std::size(cdef.CdefYPriStrength));
  for (uint32_t i = 0; i < (1 << cdef.CdefBits); i++) {
    frame_header.cdef_y_pri_strength[i] = base::span(cdef.CdefYPriStrength)[i];
    frame_header.cdef_y_sec_strength[i] = base::span(cdef.CdefYSecStrength)[i];
    frame_header.cdef_uv_pri_strength[i] =
        base::span(cdef.CdefUVPriStrength)[i];
    frame_header.cdef_uv_sec_strength[i] =
        base::span(cdef.CdefUVSecStrength)[i];
  }

  frame_header.reduced_tx_set = true;
  frame_header.segmentation_enabled = false;
  frame_header.allow_screen_content_tools =
      picture_ctrl.allow_screen_content_tools;
  frame_header.allow_intrabc = picture_ctrl.allow_intrabc;

  return frame_header;
}

aom::AV1RateControlRtcConfig ConvertToRateControlConfig(
    bool is_screen,
    const VideoBitrateAllocation& bitrate_allocation,
    const D3D12_VIDEO_ENCODER_PICTURE_RESOLUTION_DESC& resolution,
    uint32_t frame_rate,
    int num_temporal_layers) {
  aom::AV1RateControlRtcConfig rc_config{};
  // Default value from
  // //third_party/webrtc/modules/video_coding/codecs/av1/libaom_av1_encoder.cc.
  rc_config.buf_initial_sz = 600;
  rc_config.buf_optimal_sz = 600;
  rc_config.buf_sz = 1000;
  rc_config.undershoot_pct = 50;
  rc_config.overshoot_pct = 50;
  rc_config.aq_mode = 0;
  rc_config.max_intra_bitrate_pct = 50;
  rc_config.max_inter_bitrate_pct = 0;

  rc_config.width = resolution.Width;
  rc_config.height = resolution.Height;
  rc_config.target_bandwidth = bitrate_allocation.GetSumBps() / 1000.;
  rc_config.framerate = frame_rate;
  rc_config.max_quantizer = kAV1MaxQuantizer;
  rc_config.min_quantizer = kAV1MinQuantizer;

  rc_config.ss_number_layers = 1;
  rc_config.ts_number_layers = num_temporal_layers;
  int bitrate_sum = 0;
  CHECK_LT(static_cast<size_t>(rc_config.ts_number_layers),
           VideoBitrateAllocation::kMaxTemporalLayers);
  for (int tid = 0; tid < rc_config.ts_number_layers; ++tid) {
    bitrate_sum += bitrate_allocation.GetBitrateBps(0, tid);
    base::span(rc_config.layer_target_bitrate)[tid] = bitrate_sum / 1000;
    base::span(rc_config.ts_rate_decimator)[tid] =
        1u << (num_temporal_layers - tid - 1);
    base::span(rc_config.max_quantizers)[tid] = rc_config.max_quantizer;
    base::span(rc_config.min_quantizers)[tid] = rc_config.min_quantizer;
  }
  rc_config.is_screen = is_screen;
  return rc_config;
}

EncoderStatus::Or<D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAGS> GetEnabledAV1Features(
    bool is_screen,
    const D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAGS supported_features) {
  const std::array<D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAGS, 3> expected_flgs = {
      D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_CDEF_FILTERING,
      D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_ORDER_HINT_TOOLS,
      D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_REDUCED_TX_SET,
  };
  auto enabled_features = D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_NONE;
  for (const auto feature : expected_flgs) {
    if (!(supported_features & feature)) {
      return {
          EncoderStatus::Codes::kEncoderHardwareDriverError,
          base::StringPrintf(" d3d12 driver doesn't support %x .", feature)};
    }
    enabled_features |= feature;
  }

  // Enable AV1 SCC tools for screen content encoding.
  if (is_screen) {
    auto scc_tools = D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_PALETTE_ENCODING |
                     D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_INTRA_BLOCK_COPY;
    if (supported_features & scc_tools) {
      enabled_features |= scc_tools;
    }
  }
  return enabled_features;
}

D3D12VideoEncodeAV1Delegate::PictureControlFlags GetAV1PictureControl(
    bool is_keyframe,
    const D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAGS enabled_features) {
  D3D12VideoEncodeAV1Delegate::PictureControlFlags picture_ctrl;
  picture_ctrl.allow_screen_content_tools =
      enabled_features & D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_PALETTE_ENCODING;
  // D3D12 AV1 VEA only allow intra block copy for keyframes.
  picture_ctrl.allow_intrabc =
      is_keyframe ? enabled_features &
                        D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_INTRA_BLOCK_COPY
                  : false;
  return picture_ctrl;
}

}  // namespace

// static
std::vector<std::pair<VideoCodecProfile, std::vector<VideoPixelFormat>>>
D3D12VideoEncodeAV1Delegate::GetSupportedProfiles(
    ID3D12VideoDevice3* video_device) {
  CHECK(video_device);
  std::vector<std::pair<VideoCodecProfile, std::vector<VideoPixelFormat>>>
      profiles;
  D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC codec{
      .Codec = D3D12_VIDEO_ENCODER_CODEC_AV1};
  if (!CheckD3D12VideoEncoderCodec(video_device, &codec).is_ok()) {
    return profiles;
  }

  for (auto [codec_profile, av1_profile] : kVideoCodecProfileToD3D12Profile) {
    D3D12_VIDEO_ENCODER_AV1_LEVEL_TIER_CONSTRAINTS min_level;
    D3D12_VIDEO_ENCODER_AV1_LEVEL_TIER_CONSTRAINTS max_level;
    D3D12_FEATURE_DATA_VIDEO_ENCODER_PROFILE_LEVEL profile_level = {
        .Codec = D3D12_VIDEO_ENCODER_CODEC_AV1,
        .Profile = {.DataSize = sizeof(av1_profile),
                    .pAV1Profile = &av1_profile},
        .MinSupportedLevel = {.DataSize = sizeof(min_level),
                              .pAV1LevelSetting = &min_level},
        .MaxSupportedLevel = {.DataSize = sizeof(max_level),
                              .pAV1LevelSetting = &max_level},
    };
    if (!CheckD3D12VideoEncoderProfileLevel(video_device, &profile_level)
             .is_ok()) {
      continue;
    }
    std::vector<VideoPixelFormat> formats;
    for (VideoPixelFormat format : {PIXEL_FORMAT_NV12, PIXEL_FORMAT_P010LE}) {
      D3D12_FEATURE_DATA_VIDEO_ENCODER_INPUT_FORMAT input_format{
          .Codec = D3D12_VIDEO_ENCODER_CODEC_AV1,
          .Profile = profile_level.Profile,
          .Format = VideoPixelFormatToDxgiFormat(format),
      };
      if (CheckD3D12VideoEncoderInputFormat(video_device, &input_format)
              .is_ok()) {
        formats.push_back(format);
      }
    }
    if (!formats.empty()) {
      profiles.emplace_back(codec_profile, formats);
    }
  }
  return profiles;
}

D3D12VideoEncodeAV1Delegate::D3D12VideoEncodeAV1Delegate(
    Microsoft::WRL::ComPtr<ID3D12VideoDevice3> video_device)
    : D3D12VideoEncodeDelegate(std::move(video_device)) {
  input_arguments_.SequenceControlDesc.CodecGopSequence = {
      .DataSize = sizeof(gop_sequence_),
      .pAV1SequenceStructure = &gop_sequence_};
  input_arguments_.PictureControlDesc.PictureControlCodecData = {
      .DataSize = sizeof(picture_params_), .pAV1PicData = &picture_params_};
}

D3D12VideoEncodeAV1Delegate::~D3D12VideoEncodeAV1Delegate() = default;

size_t D3D12VideoEncodeAV1Delegate::GetMaxNumOfRefFrames() const {
  return max_num_ref_frames_;
}

EncoderStatus D3D12VideoEncodeAV1Delegate::InitializeVideoEncoder(
    const VideoEncodeAccelerator::Config& config) {
  DVLOG(3) << base::StringPrintf("%s: config = %s", __func__,
                                 config.AsHumanReadableString());
  CHECK_EQ(VideoCodecProfileToVideoCodec(config.output_profile),
           VideoCodec::kAV1);
  CHECK(!config.HasSpatialLayer());
  CHECK(!config.HasTemporalLayer());
  CHECK_EQ(max_num_ref_frames_, 1ull)
      << "Currently D3D12VideoEncodeAV1Delegate only support 1 reference "
         "frame.";

  if (config.bitrate.mode() != Bitrate::Mode::kConstant) {
    return {EncoderStatus::Codes::kEncoderUnsupportedConfig,
            "D3D12VideoEncoder only support CBR mode."};
  }

  D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC codec{
      .Codec = D3D12_VIDEO_ENCODER_CODEC_AV1};
  EncoderStatus status =
      CheckD3D12VideoEncoderCodec(video_device_.Get(), &codec);
  if (!status.is_ok()) {
    return status;
  }

  auto supported_profiles = GetSupportedProfiles(video_device_.Get());
  const auto supported_profile = std::ranges::find_if(
      supported_profiles,
      [config](
          const std::pair<VideoCodecProfile, std::vector<VideoPixelFormat>>&
              profile) { return profile.first == config.output_profile; });
  if (supported_profiles.end() == supported_profile) {
    return {EncoderStatus::Codes::kEncoderUnsupportedProfile,
            base::StringPrintf("D3D12VideoEncoder got unsupportted profile: %s",
                               GetProfileName(config.output_profile))};
  }
  D3D12_VIDEO_ENCODER_AV1_PROFILE profile =
      kVideoCodecProfileToD3D12Profile.at(config.output_profile);
  D3D12_VIDEO_ENCODER_AV1_CODEC_CONFIGURATION_SUPPORT config_support_limit{};
  D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT
  codec_config_support{
      .Codec = D3D12_VIDEO_ENCODER_CODEC_AV1,
      .Profile = {.DataSize = sizeof(profile), .pAV1Profile = &profile},
      .CodecSupportLimits /*output*/ = {
          .DataSize = sizeof(config_support_limit),
          .pAV1Support = &config_support_limit}};
  status = CheckD3D12VideoEncoderCodecConfigurationSupport(
      video_device_.Get(), &codec_config_support);
  if (!status.is_ok()) {
    return status;
  }

  is_screen_ = config.content_type ==
               VideoEncodeAccelerator::Config::ContentType::kDisplay;
  auto features_or_error = GetEnabledAV1Features(
      is_screen_, config_support_limit.SupportedFeatureFlags);
  if (!features_or_error.has_value()) {
    return std::move(features_or_error).error();
  }
  enabled_features_ = std::move(features_or_error).value();
  DVLOG(3) << base::StringPrintf("Enabled d3d12 encoding feature : %x.",
                                 enabled_features_);

  D3D12_VIDEO_ENCODER_AV1_CODEC_CONFIGURATION codec_config = {
      .FeatureFlags = enabled_features_,
      .OrderHintBitsMinus1 = kDefaultOrderHintBitsMinus1};

  framerate_ = config.framerate;
  bitrate_allocation_ = AllocateBitrateForDefaultEncoding(config);
  software_brc_ = aom::AV1RateControlRTC::Create(
      ConvertToRateControlConfig(is_screen_, bitrate_allocation_, input_size_,
                                 config.framerate, 1 /*num_temporal_layers_*/));

  CHECK(config.gop_length.has_value());
  gop_sequence_ = {.IntraDistance = 0,
                   .InterFramePeriod = config.gop_length.value()};

  D3D12_VIDEO_ENCODER_AV1_LEVEL_TIER_CONSTRAINTS tier_level;
  D3D12_FEATURE_DATA_VIDEO_ENCODER_RESOLUTION_SUPPORT_LIMITS
  resolution_limits[1];
  D3D12_VIDEO_ENCODER_AV1_PICTURE_CONTROL_SUBREGIONS_LAYOUT_DATA_TILES
  sub_layout{};
  cqp_pramas_ = {.ConstantQP_FullIntracodedFrame = 26,
                 .ConstantQP_InterPredictedFrame_PrevRefOnly = 30,
                 .ConstantQP_InterPredictedFrame_BiDirectionalRef = 30};
  D3D12_FEATURE_DATA_VIDEO_ENCODER_SUPPORT1 support{
      .Codec = D3D12_VIDEO_ENCODER_CODEC_AV1,
      .InputFormat = input_format_,
      .CodecConfiguration = {.DataSize = sizeof(codec_config),
                             .pAV1Config = &codec_config},
      .CodecGopSequence = {.DataSize = sizeof(gop_sequence_),
                           .pAV1SequenceStructure = &gop_sequence_},
      .RateControl = {.Mode = D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CQP,
                      .ConfigParams = {.DataSize = sizeof(cqp_pramas_),
                                       .pConfiguration_CQP = &cqp_pramas_},
                      .TargetFrameRate = {framerate_, 1}},
      .IntraRefresh = D3D12_VIDEO_ENCODER_INTRA_REFRESH_MODE_NONE,
      .SubregionFrameEncoding =
          D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_FULL_FRAME,
      .ResolutionsListCount = 1,
      .pResolutionList = &input_size_,
      .MaxReferenceFramesInDPB = max_num_ref_frames_,

      .SuggestedProfile /*output*/ = {.DataSize = sizeof(profile),
                                      .pAV1Profile = &profile},
      .SuggestedLevel /*output*/ = {.DataSize = sizeof(tier_level),
                                    .pAV1LevelSetting = &tier_level},
      .pResolutionDependentSupport = resolution_limits,
      .SubregionFrameEncodingData = {.DataSize = sizeof(sub_layout),
                                     .pTilesPartition_AV1 = &sub_layout}};
  status = CheckD3D12VideoEncoderSupport1(video_device_.Get(), &support);
  if (!status.is_ok()) {
    return status;
  }

  video_encoder_wrapper_ = video_encoder_wrapper_factory_.Run(
      video_device_.Get(), D3D12_VIDEO_ENCODER_CODEC_AV1,
      {.DataSize = sizeof(profile), .pAV1Profile = &profile},
      {.DataSize = sizeof(tier_level), .pAV1LevelSetting = &tier_level},
      input_format_,
      {.DataSize = sizeof(codec_config), .pAV1Config = &codec_config},
      input_size_);

  if (!video_encoder_wrapper_->Initialize()) {
    return {EncoderStatus::Codes::kEncoderInitializationError,
            " Failed to initialize D3D12VideoEncoderWrapper."};
  }

  if (!dpb_.InitializeTextureArray(device_.Get(), config.input_visible_size,
                                   input_format_, max_num_ref_frames_)) {
    return {EncoderStatus::Codes::kEncoderInitializationError,
            "Failed to initialize DPB."};
  }
  sequence_header_ =
      FillAV1BuilderSequenceHeader(profile, input_size_, tier_level);
  picture_id_ = -1;

  return EncoderStatus::Codes::kOk;
}

bool D3D12VideoEncodeAV1Delegate::SupportsRateControlReconfiguration() const {
  return false;
}

bool D3D12VideoEncodeAV1Delegate::UpdateRateControl(const Bitrate& bitrate,
                                                    uint32_t framerate) {
  DVLOG(3) << base::StringPrintf("%s: bitrate = %s, framerate = %d.", __func__,
                                 bitrate.ToString(), framerate);
  if (bitrate.mode() != Bitrate::Mode::kConstant) {
    LOG(ERROR) << "D3D12VideoEncoder only support AV1 "
                  "Constant bitrate mode ";
    return false;
  }
  VideoBitrateAllocation bitrate_allocation(Bitrate::Mode::kConstant);
  bitrate_allocation.SetBitrate(0, 0, bitrate.target_bps());
  if (bitrate_allocation != bitrate_allocation_ || framerate != framerate_) {
    software_brc_->UpdateRateControl(
        ConvertToRateControlConfig(is_screen_, bitrate_allocation, input_size_,
                                   framerate, 1 /*num_temporal_layers_*/));

    bitrate_allocation_ = bitrate_allocation;
    framerate_ = framerate;
  }

  return true;
}

void D3D12VideoEncodeAV1Delegate::FillPictureControlParams(
    const VideoEncoder::EncodeOptions& options) {
  CHECK(software_brc_);

  base::span picture_params_span = UNSAFE_BUFFERS(base::span(
      reinterpret_cast<uint8_t*>(&picture_params_), sizeof(picture_params_)));
  std::ranges::fill(picture_params_span, 0);
  // Update picture index and determine if a keyframe is needed.
  if (++picture_id_ == static_cast<int>(gop_sequence_.InterFramePeriod) ||
      options.key_frame) {
    picture_id_ = 0;
  }
  bool request_keyframe = picture_id_ == 0;

  picture_params_.PictureIndex = picture_id_;
  picture_ctrl_ = GetAV1PictureControl(request_keyframe, enabled_features_);
  picture_params_.Flags =
      D3D12_VIDEO_ENCODER_AV1_PICTURE_CONTROL_FLAG_REDUCED_TX_SET;
  if (picture_ctrl_.allow_screen_content_tools) {
    picture_params_.Flags |=
        D3D12_VIDEO_ENCODER_AV1_PICTURE_CONTROL_FLAG_ENABLE_PALETTE_ENCODING;
  }
  if (picture_ctrl_.allow_intrabc) {
    picture_params_.Flags |=
        D3D12_VIDEO_ENCODER_AV1_PICTURE_CONTROL_FLAG_ALLOW_INTRA_BLOCK_COPY;
  }
  picture_params_.FrameType =
      request_keyframe ? D3D12_VIDEO_ENCODER_AV1_FRAME_TYPE_KEY_FRAME
                       : D3D12_VIDEO_ENCODER_AV1_FRAME_TYPE_INTER_FRAME;
  picture_params_.CompoundPredictionType =
      D3D12_VIDEO_ENCODER_AV1_COMP_PREDICTION_TYPE_SINGLE_REFERENCE;
  picture_params_.InterpolationFilter =
      D3D12_VIDEO_ENCODER_AV1_INTERPOLATION_FILTERS_EIGHTTAP;
  picture_params_.TxMode = D3D12_VIDEO_ENCODER_AV1_TX_MODE_SELECT;
  picture_params_.SuperResDenominator = 8 /*SUPERRES_NUM*/;
  picture_params_.OrderHint =
      picture_params_.PictureIndex % (1 << (kDefaultOrderHintBitsMinus1 + 1));
  picture_params_.TemporalLayerIndexPlus1 = 0;
  picture_params_.SpatialLayerIndexPlus1 = 0;

  if (request_keyframe) {
    // When encoding a key frame, as API requirements, all array entries in
    // ReferenceFramesReconPictureDescriptors should be set to invalid index.
    for (auto& descriptor :
         picture_params_.ReferenceFramesReconPictureDescriptors) {
      descriptor.ReconstructedPictureResourceIndex = 0XFF;
    }
  }
  picture_params_.PrimaryRefFrame = request_keyframe ? kPrimaryRefNone : 0;

  // Since we only use the last frame as the reference, these should
  // always be 0.
  std::ranges::fill(picture_params_.ReferenceIndices, 0);

  // Refresh frame flags for last frame.
  picture_params_.RefreshFrameFlags =
      request_keyframe ? 0xFF : 1 << (libgav1::kReferenceFrameLast - 1);

  aom::AV1FrameParamsRTC frame_params{
      .frame_type = request_keyframe ? aom::kKeyFrame : aom::kInterFrame,
      .spatial_layer_id = 0,
      .temporal_layer_id = 0};
  software_brc_->ComputeQP(frame_params);
  int computed_qp = software_brc_->GetQP();
  picture_params_.Quantization.BaseQIndex = computed_qp;
  DVLOG(4) << base::StringPrintf(
      "Encoding picture: %d, is_keyframe = %d, QP = %d", picture_id_,
      request_keyframe, computed_qp);

  // Enable SCC tools will turn off CDEF, loop filter, etc on I-frame.
  if (!picture_ctrl_.allow_intrabc) {
    const aom::AV1LoopfilterLevel lf = software_brc_->GetLoopfilterLevel();
    base::span(picture_params_.LoopFilter.LoopFilterLevel)[0] =
        base::span(lf.filter_level)[0];
    base::span(picture_params_.LoopFilter.LoopFilterLevel)[1] =
        base::span(lf.filter_level)[1];
    picture_params_.LoopFilter.LoopFilterLevelU = lf.filter_level_u;
    picture_params_.LoopFilter.LoopFilterLevelV = lf.filter_level_v;

    auto& cdef = picture_params_.CDEF;
    cdef.CdefDampingMinus3 = 2;
    cdef.CdefBits = 3;
    for (uint32_t i = 0; i < (1 << cdef.CdefBits); i++) {
      base::span(cdef.CdefYPriStrength)[i] = kCdefYPriStrength[i];
      base::span(cdef.CdefUVPriStrength)[i] = kCdefUVPriStrength[i];
      base::span(cdef.CdefYSecStrength)[i] = KCdefYSecStrength[i];
      base::span(cdef.CdefUVSecStrength)[i] = kCdefUvSecStrength[i];
    }
  }
}

EncoderStatus::Or<BitstreamBufferMetadata>
D3D12VideoEncodeAV1Delegate::EncodeImpl(
    ID3D12Resource* input_frame,
    UINT input_frame_subresource,
    const VideoEncoder::EncodeOptions& options) {
  input_arguments_.SequenceControlDesc.Flags =
      D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_FLAG_NONE;
  input_arguments_.SequenceControlDesc.RateControl = {
      .Mode = D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CQP,
      .ConfigParams = {.DataSize = sizeof(cqp_pramas_),
                       .pConfiguration_CQP = &cqp_pramas_},
      .TargetFrameRate = {framerate_, 1}};
  input_arguments_.SequenceControlDesc.PictureTargetResolution = input_size_;

  // Fill picture_params_ for next encoded frame.
  FillPictureControlParams(options);

  bool is_keyframe =
      picture_params_.FrameType == D3D12_VIDEO_ENCODER_AV1_FRAME_TYPE_KEY_FRAME;
  input_arguments_.PictureControlDesc.Flags =
      D3D12_VIDEO_ENCODER_PICTURE_CONTROL_FLAG_USED_AS_REFERENCE_PICTURE;
  auto reconstructed_buffer = dpb_.GetCurrentFrame();
  D3D12_VIDEO_ENCODE_REFERENCE_FRAMES reference_frames{};
  if (!is_keyframe) {
    reference_frames = dpb_.ToD3D12VideoEncodeReferenceFrames();
  }
  input_arguments_.PictureControlDesc.ReferenceFrames = reference_frames;
  input_arguments_.pInputFrame = input_frame;
  input_arguments_.InputFrameSubresource = input_frame_subresource;
  D3D12_VIDEO_ENCODER_RECONSTRUCTED_PICTURE reconstructed_picture = {
      .pReconstructedPicture = reconstructed_buffer.resource_,
      .ReconstructedPictureSubresource = reconstructed_buffer.subresource_,
  };

  if (EncoderStatus result = video_encoder_wrapper_->Encode(
          input_arguments_, reconstructed_picture);
      !result.is_ok()) {
    return result;
  }

  BitstreamBufferMetadata metadata;
  metadata.key_frame = is_keyframe;
  metadata.qp = picture_params_.Quantization.BaseQIndex;
  return metadata;
}

EncoderStatus::Or<size_t> D3D12VideoEncodeAV1Delegate::ReadbackBitstream(
    base::span<uint8_t> bitstream_buffer) {
  CHECK(software_brc_);

  auto compressed_size_or_error =
      video_encoder_wrapper_->GetEncodedBitstreamWrittenBytesCount();
  if (!compressed_size_or_error.has_value()) {
    return std::move(compressed_size_or_error).error();
  }
  size_t compressed_size = std::move(compressed_size_or_error).value();
  DVLOG(4) << base::StringPrintf("%s: compressed_size = %lu", __func__,
                                 compressed_size);

  AV1BitstreamBuilder pack_header;
  // See section 5.6 of the AV1 specification.
  pack_header.WriteOBUHeader(/*type=*/libgav1::kObuTemporalDelimiter,
                             /*has_size=*/true);
  pack_header.WriteValueInLeb128(0);
  if (picture_params_.FrameType ==
      D3D12_VIDEO_ENCODER_AV1_FRAME_TYPE_KEY_FRAME) {
    // Pack sequence header OBU, see section 5.5 of the AV1 specification.
    pack_header.WriteOBUHeader(/*type=*/libgav1::kObuSequenceHeader,
                               /*has_size=*/true);
    AV1BitstreamBuilder seq_obu =
        AV1BitstreamBuilder::BuildSequenceHeaderOBU(sequence_header_);
    CHECK_EQ(seq_obu.OutstandingBits() % 8, 0ull);
    pack_header.WriteValueInLeb128(seq_obu.OutstandingBits() / 8);
    pack_header.AppendBitstreamBuffer(std::move(seq_obu));
  }
  // Pack Frame OBU, see section 5.9 of the AV1 specification.
  pack_header.WriteOBUHeader(/*type=*/libgav1::kObuFrame, /*has_size=*/true);
  AV1BitstreamBuilder frame_obu = AV1BitstreamBuilder::BuildFrameHeaderOBU(
      sequence_header_,
      FillAV1BuilderFrameHeader(picture_ctrl_, picture_params_));
  CHECK_EQ(frame_obu.OutstandingBits() % 8, 0ull);
  pack_header.WriteValueInLeb128(frame_obu.OutstandingBits() / 8 +
                                 compressed_size);
  pack_header.AppendBitstreamBuffer(std::move(frame_obu));

  std::vector<uint8_t> packed_frame_header = std::move(pack_header).Flush();
  size_t packed_header_size = packed_frame_header.size();
  bitstream_buffer.first(packed_header_size)
      .copy_from(base::span(packed_frame_header));
  auto size_or_error = D3D12VideoEncodeDelegate::ReadbackBitstream(
      bitstream_buffer.subspan(packed_header_size));
  if (!size_or_error.has_value()) {
    return std::move(size_or_error).error();
  }

  // Notify SW BRC about recent encoded frame size.
  software_brc_->PostEncodeUpdate(packed_header_size + compressed_size);

  // Refresh DPB slot 0 with current reconstructed picture.
  dpb_.ReplaceWithCurrentFrame(0);

  // Follow RefreshFrameFlags to refresh the descriptors array.
  D3D12_VIDEO_ENCODER_AV1_REFERENCE_PICTURE_DESCRIPTOR a_descriptor = {
      .ReconstructedPictureResourceIndex = 0,
      .TemporalLayerIndexPlus1 = picture_params_.TemporalLayerIndexPlus1,
      .SpatialLayerIndexPlus1 = picture_params_.SpatialLayerIndexPlus1,
      .FrameType = picture_params_.FrameType,
      .OrderHint = picture_params_.OrderHint,
      .PictureIndex = picture_params_.PictureIndex};
  base::span descriptors =
      picture_params_.ReferenceFramesReconPictureDescriptors;
  for (size_t i = 0; i < std::size(descriptors); ++i) {
    if (picture_params_.RefreshFrameFlags & (1 << i)) {
      descriptors[i] = a_descriptor;
    }
  }

  return packed_header_size + compressed_size;
}

}  // namespace media
