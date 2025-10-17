// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_video_encode_av1_delegate.h"

#include <algorithm>
#include <bit>
#include <optional>

#include "base/containers/fixed_flat_map.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
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

// See AV1 spec 7.12 for details.
constexpr std::array<int16_t, 256> kAcQuantizerLookup = {
    4,    8,    9,    10,   11,   12,   13,   14,   15,   16,   17,   18,
    19,   20,   21,   22,   23,   24,   25,   26,   27,   28,   29,   30,
    31,   32,   33,   34,   35,   36,   37,   38,   39,   40,   41,   42,
    43,   44,   45,   46,   47,   48,   49,   50,   51,   52,   53,   54,
    55,   56,   57,   58,   59,   60,   61,   62,   63,   64,   65,   66,
    67,   68,   69,   70,   71,   72,   73,   74,   75,   76,   77,   78,
    79,   80,   81,   82,   83,   84,   85,   86,   87,   88,   89,   90,
    91,   92,   93,   94,   95,   96,   97,   98,   99,   100,  101,  102,
    104,  106,  108,  110,  112,  114,  116,  118,  120,  122,  124,  126,
    128,  130,  132,  134,  136,  138,  140,  142,  144,  146,  148,  150,
    152,  155,  158,  161,  164,  167,  170,  173,  176,  179,  182,  185,
    188,  191,  194,  197,  200,  203,  207,  211,  215,  219,  223,  227,
    231,  235,  239,  243,  247,  251,  255,  260,  265,  270,  275,  280,
    285,  290,  295,  300,  305,  311,  317,  323,  329,  335,  341,  347,
    353,  359,  366,  373,  380,  387,  394,  401,  408,  416,  424,  432,
    440,  448,  456,  465,  474,  483,  492,  501,  510,  520,  530,  540,
    550,  560,  571,  582,  593,  604,  615,  627,  639,  651,  663,  676,
    689,  702,  715,  729,  743,  757,  771,  786,  801,  816,  832,  848,
    864,  881,  898,  915,  933,  951,  969,  988,  1007, 1026, 1046, 1066,
    1087, 1108, 1129, 1151, 1173, 1196, 1219, 1243, 1267, 1292, 1317, 1343,
    1369, 1396, 1423, 1451, 1479, 1508, 1537, 1567, 1597, 1628, 1660, 1692,
    1725, 1759, 1793, 1828,
};

uint8_t AV1QPtoQindex(uint8_t avenc_qp) {
  uint8_t q_index = avenc_qp * 4;
  if (q_index == 248) {
    q_index = 249;
  } else if (q_index == 252) {
    q_index = 255;
  }
  return q_index;
}

AV1BitstreamBuilder::SequenceHeader FillAV1BuilderSequenceHeader(
    uint8_t num_temporal_layers,
    D3D12_VIDEO_ENCODER_AV1_PROFILE profile,
    const D3D12_VIDEO_ENCODER_PICTURE_RESOLUTION_DESC& input_size,
    const D3D12_VIDEO_ENCODER_AV1_LEVEL_TIER_CONSTRAINTS& tier_level,
    const D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAGS& enabled_features) {
  AV1BitstreamBuilder::SequenceHeader sequence_header{};

  sequence_header.profile = profile;
  sequence_header.operating_points_cnt_minus_1 = num_temporal_layers - 1;
  for (uint8_t i = 0; i <= sequence_header.operating_points_cnt_minus_1; i++) {
    sequence_header.level[i] = tier_level.Level;
    sequence_header.tier[i] = tier_level.Tier;
  }
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
  sequence_header.enable_restoration =
      !!(enabled_features &
         D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_LOOP_RESTORATION_FILTER);

  return sequence_header;
}

AV1BitstreamBuilder::FrameHeader FillAV1BuilderFrameHeader(
    const D3D12VideoEncodeAV1Delegate::PictureControlFlags& picture_ctrl,
    const D3D12_VIDEO_ENCODER_AV1_PICTURE_CONTROL_CODEC_DATA& pic_params,
    const AV1BitstreamBuilder::SequenceHeader& sequence_header) {
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
  frame_header.cdef_damping_minus_3 = cdef.CdefDampingMinus3 & 0x3;
  frame_header.cdef_bits = cdef.CdefBits;
  for (uint32_t i = 0; i < (1 << cdef.CdefBits); i++) {
    frame_header.cdef_y_pri_strength[i] = base::span(cdef.CdefYPriStrength)[i];
    frame_header.cdef_y_sec_strength[i] = base::span(cdef.CdefYSecStrength)[i];
    frame_header.cdef_uv_pri_strength[i] =
        base::span(cdef.CdefUVPriStrength)[i];
    frame_header.cdef_uv_sec_strength[i] =
        base::span(cdef.CdefUVSecStrength)[i];
  }

  frame_header.tx_mode = pic_params.TxMode;
  frame_header.reduced_tx_set = false;
  frame_header.segmentation_enabled = picture_ctrl.enable_auto_segmentation;
  frame_header.allow_screen_content_tools =
      picture_ctrl.allow_screen_content_tools;
  frame_header.allow_intrabc = picture_ctrl.allow_intrabc;
  frame_header.interpolation_filter =
      static_cast<libgav1::InterpolationFilter>(pic_params.InterpolationFilter);

  // When loop restoration is enabled, updates frame header with loop
  // restoration parameters submitted to driver.
  if (sequence_header.enable_restoration) {
    const auto& restoration_config = pic_params.FrameRestorationConfig;
    uint8_t lr_unit_shift = 0;
    uint8_t lr_uv_shift = 0;

    frame_header.restoration_type[0] =
        static_cast<libgav1::LoopRestorationType>(
            restoration_config.FrameRestorationType[0]);
    frame_header.restoration_type[1] =
        static_cast<libgav1::LoopRestorationType>(
            restoration_config.FrameRestorationType[1]);
    frame_header.restoration_type[2] =
        static_cast<libgav1::LoopRestorationType>(
            restoration_config.FrameRestorationType[2]);
    // Calculate the lr_unit_shift that shall be used. 64 * 2^lr_unit_shift
    // is the size of the loop restoration tile size in pixels.
    auto restoration_y_tile_size =
        restoration_config.LoopRestorationPixelSize[0];
    auto restoration_u_tile_size =
        restoration_config.LoopRestorationPixelSize[1];
    auto resotration_v_tile_size =
        restoration_config.LoopRestorationPixelSize[2];
    auto restoration_size_max = std::max({
        restoration_y_tile_size,
        restoration_u_tile_size,
        resotration_v_tile_size,
    });
    switch (restoration_size_max) {
      case D3D12_VIDEO_ENCODER_AV1_RESTORATION_TILESIZE_256x256:
        lr_unit_shift = 2;
        break;
      case D3D12_VIDEO_ENCODER_AV1_RESTORATION_TILESIZE_128x128:
        lr_unit_shift = 1;
        break;
      case D3D12_VIDEO_ENCODER_AV1_RESTORATION_TILESIZE_64x64:
      case D3D12_VIDEO_ENCODER_AV1_RESTORATION_TILESIZE_DISABLED:
        lr_unit_shift = 0;
        break;
      default:
        NOTREACHED();
    }
    // Check if either restoration_u_tile_size or resotration_v_tile_size is
    // equal to resotration_y_tile_size, if so, lr_uv_shift is 0; otherwise,
    // lr_uv_shift should be 1.
    if (restoration_u_tile_size == restoration_y_tile_size ||
        resotration_v_tile_size == restoration_y_tile_size) {
      lr_uv_shift = 0;
    } else {
      lr_uv_shift = 1;
    }

    frame_header.lr_unit_shift = lr_unit_shift;
    frame_header.lr_uv_shift = lr_uv_shift;
  }

  return frame_header;
}

// Helper function to print the D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES.
std::string PrintPostEncodeValues(
    const D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES& post_encode_values) {
  auto join_uint8_array = [](const auto& arr) {
    std::string result;
    for (size_t i = 0; i < std::size(arr); ++i) {
      if (i > 0) {
        result += ", ";
      }
      result += base::NumberToString(static_cast<int32_t>(arr[i]));
    }
    return result;
  };

  auto print_segments_enabled_features = [&]() {
    std::string segs;
    for (size_t i = 0;
         i < std::size(post_encode_values.SegmentationConfig.SegmentsData);
         ++i) {
      segs += "\n  [" + base::NumberToString(i) + "]: ";
      segs += base::NumberToString(static_cast<uint32_t>(
          base::span(post_encode_values.SegmentationConfig.SegmentsData)[i]
              .EnabledFeatures));
      segs += " FeatureValue[";
      for (size_t j = 0;
           j <
           std::size(
               base::span(post_encode_values.SegmentationConfig.SegmentsData)[i]
                   .FeatureValue);
           ++j) {
        if (j > 0) {
          segs += ", ";
        }
        segs += base::NumberToString(static_cast<int32_t>(base::span(
            base::span(post_encode_values.SegmentationConfig.SegmentsData)[i]
                .FeatureValue)[j]));
      }
      segs += "]";
    }
    return segs;
  };

  auto print_reference_indices = [&]() {
    std::string refs;
    for (size_t i = 0; i < std::size(post_encode_values.ReferenceIndices);
         ++i) {
      refs += "\n  [" + base::NumberToString(i) + "]: ";
      refs += base::NumberToString(static_cast<uint32_t>(
          base::span(post_encode_values.ReferenceIndices)[i]));
    }
    return refs;
  };

  return base::StringPrintf(
      "\n[Post Encode Values]:\n"
      "CDEF:\n"
      "  CdefBits=%llu\n"
      "  CdefDampingMinus3=%llu\n"
      "  CdefYPriStrength=%s\n"
      "  CdefYSecStrength=%s\n"
      "  CdefUVPriStrength=%s\n"
      "  CdefUVSecStrength=%s\n"
      "LoopFilter:\n"
      "  LoopFilterLevel=%s\n"
      "  LoopFilterLevelU=%llu\n"
      "  LoopFilterLevelV=%llu\n"
      "  LoopFilterSharpnessLevel=%llu\n"
      "  LoopFilterDeltaEnabled=%llu\n"
      "  UpdateRefDelta=%llu\n"
      "  RefDeltas=%s\n"
      "  UpdateModeDelta=%llu\n"
      "  ModeDeltas=%s\n"
      "Quantization:\n"
      "  BaseQIndex=%llu\n"
      "  YDCDeltaQ=%lld\n"
      "  UDCDeltaQ=%lld\n"
      "  UACDeltaQ=%lld\n"
      "  VDCDeltaQ=%lld\n"
      "  VACDeltaQ=%lld\n"
      "QuantizationDelta:\n"
      "  DeltaQPresent=%llu\n"
      "  DeltaQRes=%llu\n"
      "CompoundPredictionType: %llu\n"
      "SegmentationConfig:\n"
      "  NumSegments=%llu\n"
      "  UpdateMap=%llu\n"
      "  TemporalUpdate=%llu\n"
      "  UpdateData=%llu\n"
      "  SegmentsData.EnabledFeatures:%s\n"
      "PrimaryRefFrame: %llu\n"
      "ReferenceIndices:%s\n",
      post_encode_values.CDEF.CdefBits,
      post_encode_values.CDEF.CdefDampingMinus3,
      join_uint8_array(post_encode_values.CDEF.CdefYPriStrength).c_str(),
      join_uint8_array(post_encode_values.CDEF.CdefYSecStrength).c_str(),
      join_uint8_array(post_encode_values.CDEF.CdefUVPriStrength).c_str(),
      join_uint8_array(post_encode_values.CDEF.CdefUVSecStrength).c_str(),
      join_uint8_array(post_encode_values.LoopFilter.LoopFilterLevel).c_str(),
      post_encode_values.LoopFilter.LoopFilterLevelU,
      post_encode_values.LoopFilter.LoopFilterLevelV,
      post_encode_values.LoopFilter.LoopFilterSharpnessLevel,
      post_encode_values.LoopFilter.LoopFilterDeltaEnabled,
      post_encode_values.LoopFilter.UpdateRefDelta,
      join_uint8_array(post_encode_values.LoopFilter.RefDeltas).c_str(),
      post_encode_values.LoopFilter.UpdateModeDelta,
      join_uint8_array(post_encode_values.LoopFilter.ModeDeltas).c_str(),
      post_encode_values.Quantization.BaseQIndex,
      post_encode_values.Quantization.YDCDeltaQ,
      post_encode_values.Quantization.UDCDeltaQ,
      post_encode_values.Quantization.UACDeltaQ,
      post_encode_values.Quantization.VDCDeltaQ,
      post_encode_values.Quantization.VACDeltaQ,
      static_cast<uint64_t>(post_encode_values.QuantizationDelta.DeltaQPresent),
      static_cast<uint64_t>(post_encode_values.QuantizationDelta.DeltaQRes),
      static_cast<uint64_t>(post_encode_values.CompoundPredictionType),
      static_cast<uint64_t>(post_encode_values.SegmentationConfig.NumSegments),
      static_cast<uint64_t>(post_encode_values.SegmentationConfig.UpdateMap),
      static_cast<uint64_t>(
          post_encode_values.SegmentationConfig.TemporalUpdate),
      static_cast<uint64_t>(post_encode_values.SegmentationConfig.UpdateData),
      print_segments_enabled_features().c_str(),
      static_cast<uint64_t>(post_encode_values.PrimaryRefFrame),
      print_reference_indices().c_str());
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
    const D3D12_VIDEO_ENCODER_AV1_CODEC_CONFIGURATION_SUPPORT&
        config_support_limit) {
  const auto& supported_features = config_support_limit.SupportedFeatureFlags;
  const auto& required_features = config_support_limit.RequiredFeatureFlags;
  if ((supported_features & required_features) != required_features) {
    return {EncoderStatus::Codes::kEncoderHardwareDriverError,
            base::StringPrintf(" d3d12 driver doesn't support %x .",
                               required_features)};
  }
  const std::array<D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAGS, 2> expected_flgs = {
      D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_CDEF_FILTERING,
      D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_ORDER_HINT_TOOLS,
  };
  auto enabled_features = required_features;
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
    if ((supported_features & scc_tools) == scc_tools) {
      enabled_features |= scc_tools;
    }
  }
  return enabled_features;
}

std::pair<D3D12_VIDEO_ENCODER_AV1_RESTORATION_TYPE,
          D3D12_VIDEO_ENCODER_AV1_RESTORATION_TILESIZE>
SelectBestRestoration(
    base::span<const D3D12_VIDEO_ENCODER_AV1_RESTORATION_SUPPORT_FLAGS>
        supported_per_type) {
  // Prefer WIENER, then SGRPROJ, finally SWITCHABLE.
  // For each, prefer the largest supported restoration tile size.
  static constexpr std::array<D3D12_VIDEO_ENCODER_AV1_RESTORATION_TYPE, 3>
      restoration_type_preferences = {
          D3D12_VIDEO_ENCODER_AV1_RESTORATION_TYPE_WIENER,
          D3D12_VIDEO_ENCODER_AV1_RESTORATION_TYPE_SGRPROJ,
          D3D12_VIDEO_ENCODER_AV1_RESTORATION_TYPE_SWITCHABLE};
  static constexpr std::array<
      std::pair<D3D12_VIDEO_ENCODER_AV1_RESTORATION_SUPPORT_FLAGS,
                D3D12_VIDEO_ENCODER_AV1_RESTORATION_TILESIZE>,
      4>
      restoration_tile_size_preferences = {{
          {D3D12_VIDEO_ENCODER_AV1_RESTORATION_SUPPORT_FLAG_256x256,
           D3D12_VIDEO_ENCODER_AV1_RESTORATION_TILESIZE_256x256},
          {D3D12_VIDEO_ENCODER_AV1_RESTORATION_SUPPORT_FLAG_128x128,
           D3D12_VIDEO_ENCODER_AV1_RESTORATION_TILESIZE_128x128},
          {D3D12_VIDEO_ENCODER_AV1_RESTORATION_SUPPORT_FLAG_64x64,
           D3D12_VIDEO_ENCODER_AV1_RESTORATION_TILESIZE_64x64},
          {D3D12_VIDEO_ENCODER_AV1_RESTORATION_SUPPORT_FLAG_32x32,
           D3D12_VIDEO_ENCODER_AV1_RESTORATION_TILESIZE_32x32},
      }};
  // supported_per_type[0]=>SWITCHABLE's masks, [1]=>WIENER's masks,
  // [2]=>SGRPROJ's masks.
  for (const auto& type : restoration_type_preferences) {
    uint32_t mask =
        supported_per_type[type -
                           D3D12_VIDEO_ENCODER_AV1_RESTORATION_TYPE_SWITCHABLE];
    for (const auto& pref : restoration_tile_size_preferences) {
      if (mask & pref.first) {
        return std::make_pair(type, pref.second);
      }
    }
  }
  return std::make_pair(D3D12_VIDEO_ENCODER_AV1_RESTORATION_TYPE_DISABLED,
                        D3D12_VIDEO_ENCODER_AV1_RESTORATION_TILESIZE_DISABLED);
}

EncoderStatus::Or<D3D12VideoEncodeAV1Delegate::D3D12EncodingCapabilities>
GetAV1EncodingCapabilities(
    const D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAGS enabled_features,
    const D3D12_VIDEO_ENCODER_AV1_CODEC_CONFIGURATION_SUPPORT&
        config_support_limit) {
  D3D12VideoEncodeAV1Delegate::D3D12EncodingCapabilities encoding_caps{};
  encoding_caps.post_value_flags = config_support_limit.PostEncodeValuesFlags;

  static constexpr std::array kInterpolationFilters = {
      D3D12_VIDEO_ENCODER_AV1_INTERPOLATION_FILTERS_EIGHTTAP,
      D3D12_VIDEO_ENCODER_AV1_INTERPOLATION_FILTERS_EIGHTTAP_SMOOTH,
      D3D12_VIDEO_ENCODER_AV1_INTERPOLATION_FILTERS_EIGHTTAP_SHARP,
      D3D12_VIDEO_ENCODER_AV1_INTERPOLATION_FILTERS_BILINEAR,
      D3D12_VIDEO_ENCODER_AV1_INTERPOLATION_FILTERS_SWITCHABLE,
  };
  auto interpolation_filter_it = std::ranges::find_if(
      kInterpolationFilters, [config_support_limit](int filter) {
        return config_support_limit.SupportedInterpolationFilters &
               (1u << filter);
      });
  if (interpolation_filter_it == kInterpolationFilters.end()) {
    return {EncoderStatus::Codes::kEncoderInitializationError,
            "No interpolation filters available."};
  }
  encoding_caps.interpolation_filter = *interpolation_filter_it;

  const base::span supported_tx_modes = config_support_limit.SupportedTxModes;
  for (int i = 0; i < 2; ++i) {
    if (supported_tx_modes[i] & D3D12_VIDEO_ENCODER_AV1_TX_MODE_FLAG_SELECT) {
      encoding_caps.tx_modes[i] = D3D12_VIDEO_ENCODER_AV1_TX_MODE_SELECT;
    } else {
      encoding_caps.tx_modes[i] = D3D12_VIDEO_ENCODER_AV1_TX_MODE_LARGEST;
    }
  }

  base::span restoration_type =
      encoding_caps.loop_restoration.FrameRestorationType;
  base::span restoration_pixel_size =
      encoding_caps.loop_restoration.LoopRestorationPixelSize;
  if (enabled_features &
      D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_LOOP_RESTORATION_FILTER) {
    // Layout of SupportedRestorationParams:
    // SupportedRestorationParams[restoration_type][plane]
    // restoration_type: 0=SWITCHABLE, 1=WIENER, 2=SGRPROJ
    // plane: 0=Y, 1=U, 2=V
    for (int plane = 0; plane < 3; ++plane) {
      const std::array<D3D12_VIDEO_ENCODER_AV1_RESTORATION_SUPPORT_FLAGS, 3>
          supported_per_type = {
              // SAFETY: `config_support_limit.SupportedRestorationParams` is
              // a 3x3 2D array the AV1 delegte creates as part of initializing
              // `config_support_limit`, and is later filled in by driver
              // during encoder feature check, so accessing through [0..2][0..2]
              // is safe.
              UNSAFE_BUFFERS(
                  config_support_limit.SupportedRestorationParams[0][plane]),
              UNSAFE_BUFFERS(
                  config_support_limit.SupportedRestorationParams[1][plane]),
              UNSAFE_BUFFERS(
                  config_support_limit.SupportedRestorationParams[2][plane])};
      const auto& [type, tile_size] =
          SelectBestRestoration(base::span(supported_per_type));
      restoration_type[plane] = type;
      restoration_pixel_size[plane] = tile_size;
    }
  } else {
    std::ranges::fill(restoration_type,
                      D3D12_VIDEO_ENCODER_AV1_RESTORATION_TYPE_DISABLED);
    std::ranges::fill(restoration_pixel_size,
                      D3D12_VIDEO_ENCODER_AV1_RESTORATION_TILESIZE_DISABLED);
  }

  return encoding_caps;
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
  picture_ctrl.enable_auto_segmentation =
      enabled_features & D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_AUTO_SEGMENTATION;
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
    for (VideoPixelFormat format :
         {PIXEL_FORMAT_NV12, PIXEL_FORMAT_P010LE, PIXEL_FORMAT_ABGR}) {
      D3D12_FEATURE_DATA_VIDEO_ENCODER_INPUT_FORMAT input_format{
          .Codec = D3D12_VIDEO_ENCODER_CODEC_AV1,
          .Profile = profile_level.Profile,
          // It makes no sense to encode at Y:U:V 444 if input UV is already
          // subsampled. So only allow profile 1 encoding when input is RGBA
          // frame, and converts to AYUV which is the only DXGI format supported
          // by drivers at 8b profile 1.
          .Format = (format == PIXEL_FORMAT_ABGR)
                        ? DXGI_FORMAT_AYUV
                        : VideoPixelFormatToDxgiFormat(format),
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

size_t D3D12VideoEncodeAV1Delegate::GetMaxNumOfManualRefBuffers() const {
  // We should have initialized.
  CHECK_GT(max_num_ref_frames_, 0u);
  return max_num_ref_frames_;
}

bool D3D12VideoEncodeAV1Delegate::ReportsAverageQp() const {
  return true;
}

EncoderStatus D3D12VideoEncodeAV1Delegate::InitializeVideoEncoder(
    const VideoEncodeAccelerator::Config& config) {
  DVLOG(3) << base::StringPrintf("%s: config = %s", __func__,
                                 config.AsHumanReadableString());
  CHECK_EQ(VideoCodecProfileToVideoCodec(config.output_profile),
           VideoCodec::kAV1);
  CHECK(!config.HasSpatialLayer());

  // For L1T3, we need two reference frames (for T0 and T1 frames).
  // For L1T1  and L1T2, one reference frame is sufficient.
  max_num_ref_frames_ = GetNumTemporalLayers() == 3 ? 2 : 1;

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
  auto features_or_error =
      GetEnabledAV1Features(is_screen_, config_support_limit);
  if (!features_or_error.has_value()) {
    return std::move(features_or_error).error();
  }
  enabled_features_ = std::move(features_or_error).value();
  DVLOG(3) << base::StringPrintf("Enabled d3d12 encoding feature : %x.",
                                 enabled_features_);

  auto enc_caps_or_error =
      GetAV1EncodingCapabilities(enabled_features_, config_support_limit);
  if (!enc_caps_or_error.has_value()) {
    return std::move(enc_caps_or_error).error();
  }
  enc_caps_ = std::move(enc_caps_or_error).value();

  D3D12_VIDEO_ENCODER_AV1_CODEC_CONFIGURATION codec_config = {
      .FeatureFlags = enabled_features_,
      .OrderHintBitsMinus1 = kDefaultOrderHintBitsMinus1};

  if (config.bitrate.mode() == Bitrate::Mode::kConstant ||
      config.bitrate.mode() == Bitrate::Mode::kVariable) {
    software_brc_ = aom::AV1RateControlRTC::Create(
        ConvertToRateControlConfig(is_screen_, bitrate_allocation_, input_size_,
                                   config.framerate, GetNumTemporalLayers()));
    rate_control_ = D3D12VideoEncoderRateControl::CreateCqp(
        26 /*i_frame_qp*/, 30 /*p_frame_qp*/, 30 /*b_frame_qp*/);
  }

  CHECK(config.gop_length.has_value());
  gop_sequence_ = {.IntraDistance = 0,
                   .InterFramePeriod = config.gop_length.value()};

  D3D12_VIDEO_ENCODER_AV1_LEVEL_TIER_CONSTRAINTS tier_level;
  D3D12_FEATURE_DATA_VIDEO_ENCODER_RESOLUTION_SUPPORT_LIMITS
  resolution_limits[1];
  D3D12_FEATURE_DATA_VIDEO_ENCODER_SUPPORT1 support{
      .Codec = D3D12_VIDEO_ENCODER_CODEC_AV1,
      .InputFormat = input_format_,
      .CodecConfiguration = {.DataSize = sizeof(codec_config),
                             .pAV1Config = &codec_config},
      .CodecGopSequence = {.DataSize = sizeof(gop_sequence_),
                           .pAV1SequenceStructure = &gop_sequence_},
      .RateControl = rate_control_.GetD3D12VideoEncoderRateControl(),
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
      .SubregionFrameEncodingData = {.DataSize = sizeof(sub_layout_),
                                     .pTilesPartition_AV1 = &sub_layout_}};
  status = CheckD3D12VideoEncoderSupport1(video_device_.Get(), &support);
  if (auto subregion_block_size =
          resolution_limits[0].SubregionBlockPixelsSize) {
    sub_layout_.ColWidths[0] =
        (input_size_.Width + subregion_block_size - 1) / subregion_block_size;
    sub_layout_.RowHeights[0] =
        (input_size_.Height + subregion_block_size - 1) / subregion_block_size;
    sub_layout_.RowCount = 1;
    sub_layout_.ColCount = 1;
    sub_layout_.ContextUpdateTileId = 0;
  }
  if (support.ValidationFlags &
      D3D12_VIDEO_ENCODER_VALIDATION_FLAG_SUBREGION_LAYOUT_DATA_NOT_SUPPORTED) {
    support.SubregionFrameEncodingData = {.DataSize = sizeof(sub_layout_),
                                          .pTilesPartition_AV1 = &sub_layout_};
    status = CheckD3D12VideoEncoderSupport1(video_device_.Get(), &support);
  }

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

  // We use full frame mode so the number of subregions is always 1.
  if (!video_encoder_wrapper_->Initialize(/*max_subregions_number=*/1)) {
    return {EncoderStatus::Codes::kEncoderInitializationError,
            " Failed to initialize D3D12VideoEncoderWrapper."};
  }

  bool use_texture_array =
      support.SupportFlags &
      D3D12_VIDEO_ENCODER_SUPPORT_FLAG_RECONSTRUCTED_FRAMES_REQUIRE_TEXTURE_ARRAYS;
  if (!dpb_.InitializeTextureResources(device_.Get(), config.input_visible_size,
                                       input_format_, max_num_ref_frames_,
                                       use_texture_array)) {
    return {EncoderStatus::Codes::kEncoderInitializationError,
            "Failed to initialize DPB."};
  }
  if (svc_layers_) {
    metadata_.svc_generic.emplace();
  }
  sequence_header_ =
      FillAV1BuilderSequenceHeader(GetNumTemporalLayers(), profile, input_size_,
                                   tier_level, enabled_features_);
  picture_id_ = -1;
  current_rate_control_ = rate_control_;

  return EncoderStatus::Codes::kOk;
}

bool D3D12VideoEncodeAV1Delegate::SupportsRateControlReconfiguration() const {
  return false;
}

bool D3D12VideoEncodeAV1Delegate::UpdateRateControl(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate) {
  DVLOG(3) << base::StringPrintf("%s: bitrate = %s, framerate = %d.", __func__,
                                 bitrate_allocation.ToString(), framerate);
  if (!software_brc_) {
    return D3D12VideoEncodeDelegate::UpdateRateControl(bitrate_allocation,
                                                       framerate);
  }

  if (bitrate_allocation.GetMode() != Bitrate::Mode::kConstant &&
      bitrate_allocation.GetMode() != Bitrate::Mode::kVariable) {
    return false;
  }

  if (bitrate_allocation != bitrate_allocation_ || framerate != framerate_) {
    software_brc_->UpdateRateControl(
        ConvertToRateControlConfig(is_screen_, bitrate_allocation, input_size_,
                                   framerate, GetNumTemporalLayers()));

    bitrate_allocation_ = bitrate_allocation;
    framerate_ = framerate;
  }

  return true;
}

void D3D12VideoEncodeAV1Delegate::FillPictureControlParams(
    const VideoEncoder::EncodeOptions& options) {
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
  picture_params_.Flags = D3D12_VIDEO_ENCODER_AV1_PICTURE_CONTROL_FLAG_NONE;
  if (picture_ctrl_.allow_screen_content_tools) {
    picture_params_.Flags |=
        D3D12_VIDEO_ENCODER_AV1_PICTURE_CONTROL_FLAG_ENABLE_PALETTE_ENCODING;
  }
  if (picture_ctrl_.allow_intrabc) {
    picture_params_.Flags |=
        D3D12_VIDEO_ENCODER_AV1_PICTURE_CONTROL_FLAG_ALLOW_INTRA_BLOCK_COPY;
  }
  if (picture_ctrl_.enable_auto_segmentation) {
    picture_params_.Flags |=
        D3D12_VIDEO_ENCODER_AV1_PICTURE_CONTROL_FLAG_ENABLE_FRAME_SEGMENTATION_AUTO;
  }
  picture_params_.FrameType =
      request_keyframe ? D3D12_VIDEO_ENCODER_AV1_FRAME_TYPE_KEY_FRAME
                       : D3D12_VIDEO_ENCODER_AV1_FRAME_TYPE_INTER_FRAME;
  picture_params_.CompoundPredictionType =
      D3D12_VIDEO_ENCODER_AV1_COMP_PREDICTION_TYPE_SINGLE_REFERENCE;
  picture_params_.InterpolationFilter = enc_caps_.interpolation_filter;
  picture_params_.TxMode = enc_caps_.tx_modes[request_keyframe ? 0 : 1];
  picture_params_.FrameRestorationConfig = enc_caps_.loop_restoration;

  picture_params_.SuperResDenominator = 8 /*SUPERRES_NUM*/;
  picture_params_.OrderHint =
      picture_params_.PictureIndex % (1 << (kDefaultOrderHintBitsMinus1 + 1));
  picture_params_.TemporalLayerIndexPlus1 = 0;
  picture_params_.SpatialLayerIndexPlus1 = 0;

  if (request_keyframe) {
    // When encoding a key frame, as API requirements, all array entries in
    // ReferenceFramesReconPictureDescriptors should be set to invalid index.
    reference_descriptors_.fill({.ReconstructedPictureResourceIndex = 0xFF});
    picture_params_.PrimaryRefFrame = kPrimaryRefNone;
  }
  std::copy(reference_descriptors_.begin(), reference_descriptors_.end(),
            picture_params_.ReferenceFramesReconPictureDescriptors);

  if (svc_layers_) {
    CHECK(metadata_.svc_generic.has_value());
    // If keyframe is requested, then reset |svc_layers_|.
    if (request_keyframe) {
      svc_layers_->Reset();
    }
    SVCLayers::PictureParam svc_layer_params{};
    svc_layers_->GetPictureParamAndMetadata(svc_layer_params,
                                            &metadata_.svc_generic.value());
    picture_params_.RefreshFrameFlags = svc_layer_params.refresh_frame_flags;
    if (!request_keyframe) {
      CHECK_EQ(svc_layer_params.reference_frame_indices.size(), 1ull);
      std::ranges::fill(picture_params_.ReferenceIndices,
                        svc_layer_params.reference_frame_indices[0]);
      picture_params_.PrimaryRefFrame =
          svc_layer_params.reference_frame_indices[0];
    }
  } else {
    // TODO(https://crbug.com/40275246): Support manual reference control
    // indicated in 'EncodeOptions'.

    // If there is no outside reference control, we use the last frame as the
    // reference frame for inter frames.
    picture_params_.PrimaryRefFrame = request_keyframe ? kPrimaryRefNone : 0;
    std::ranges::fill(picture_params_.ReferenceIndices, 0);

    // Refresh frame flags for last frame.
    picture_params_.RefreshFrameFlags =
        request_keyframe ? 0xFF : 1 << (libgav1::kReferenceFrameLast - 1);
  }

  std::optional<int> qindex;
  if (software_brc_) {
    aom::AV1FrameParamsRTC frame_params{
        .frame_type = request_keyframe ? aom::kKeyFrame : aom::kInterFrame,
        .spatial_layer_id = 0,
        .temporal_layer_id =
            metadata_.svc_generic ? metadata_.svc_generic->temporal_idx : 0};
    software_brc_->ComputeQP(frame_params);
    qindex = software_brc_->GetQP();
  } else if (options.quantizer.has_value()) {
    qindex = AV1QPtoQindex(
        std::clamp(static_cast<uint8_t>(options.quantizer.value()),
                   kAV1MinQuantizer, kAV1MaxQuantizer));
  }
  const int base_q_idx =
      std::clamp(qindex.value_or(AV1QPtoQindex(kAV1MaxQuantizer)), 0, 255);
  picture_params_.Quantization.BaseQIndex = base_q_idx;
  DVLOG(4) << base::StringPrintf(
      "Encoding picture: %d, is_keyframe = %d, QP = %d", picture_id_,
      request_keyframe, base_q_idx);

  if (qindex.has_value()) {
    CHECK_EQ(input_arguments_.SequenceControlDesc.RateControl.Mode,
             D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CQP);
    current_rate_control_.SetCQP(
        request_keyframe ? D3D12VideoEncoderRateControl::FrameType::kIntra
                         : D3D12VideoEncoderRateControl::FrameType::kInterPrev,
        qindex.value());
    input_arguments_.SequenceControlDesc.RateControl =
        current_rate_control_.GetD3D12VideoEncoderRateControl();
  } else if (rate_control_ != current_rate_control_) {
    if (rate_control_.GetMode() != current_rate_control_.GetMode()) {
      CHECK(SupportsRateControlReconfiguration());
      input_arguments_.SequenceControlDesc.Flags |=
          D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_FLAG_RATE_CONTROL_CHANGE;
    }
    current_rate_control_ = rate_control_;
    input_arguments_.SequenceControlDesc.RateControl =
        current_rate_control_.GetD3D12VideoEncoderRateControl();
  }

  // Enable SCC tools will turn off CDEF, loop filter, etc on I-frame.
  if (!picture_ctrl_.allow_intrabc) {
    if (software_brc_) {
      const aom::AV1LoopfilterLevel lf = software_brc_->GetLoopfilterLevel();
      base::span(picture_params_.LoopFilter.LoopFilterLevel)[0] =
          base::span(lf.filter_level)[0];
      base::span(picture_params_.LoopFilter.LoopFilterLevel)[1] =
          base::span(lf.filter_level)[1];
      picture_params_.LoopFilter.LoopFilterLevelU = lf.filter_level_u;
      picture_params_.LoopFilter.LoopFilterLevelV = lf.filter_level_v;
    } else {
      // Calculate loop filter levels based on libaom's approach from
      // //third_party/libaom/source/libaom/av1/encoder/picklpf.c.
      // These values were determined by linear fitting the result of the
      // searched level for 8 bit depth:
      // Keyframes: filt_guess = q * 0.06699 - 1.60817
      // Other frames: filt_guess = q * inter_frame_multiplier + 2.48225
      int filter_level = 0;
      const int q = kAcQuantizerLookup[base_q_idx];
      int inter_frame_multiplier =
          input_size_.Width * input_size_.Height > 352 * 288 ? 12034 : 6017;
      // Convert to fixed point: 0.06699 ≈ 17563/262144, -1.60817 ≈
      // -421574/262144, 2.48225 ≈ 650707/26214.
      if (request_keyframe) {
        filter_level = (q * 17563 - 421574 + (1 << 17)) >> 18;
      } else {
        filter_level = (q * inter_frame_multiplier + 650707 + (1 << 17)) >> 18;
      }
      filter_level = std::clamp(filter_level, 0, 63);
      picture_params_.LoopFilter.LoopFilterLevel[0] = filter_level;
      picture_params_.LoopFilter.LoopFilterLevel[1] = filter_level;
      if (filter_level > 0) {
        picture_params_.LoopFilter.LoopFilterLevelU = filter_level;
        picture_params_.LoopFilter.LoopFilterLevelV = filter_level;
      }
    }

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

EncoderStatus D3D12VideoEncodeAV1Delegate::EncodeImpl(
    ID3D12Resource* input_frame,
    UINT input_frame_subresource,
    const VideoEncoder::EncodeOptions& options,
    const gfx::ColorSpace& input_color_space) {
  input_arguments_.SequenceControlDesc.Flags =
      D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_FLAG_NONE;
  input_arguments_.SequenceControlDesc.RateControl =
      rate_control_.GetD3D12VideoEncoderRateControl();
  input_arguments_.SequenceControlDesc.PictureTargetResolution = input_size_;
  input_arguments_.SequenceControlDesc.SelectedLayoutMode =
      D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_FULL_FRAME;
  input_arguments_.SequenceControlDesc.FrameSubregionsLayoutData = {
      .DataSize = sizeof(sub_layout_), .pTilesPartition_AV1 = &sub_layout_};

  // Fill picture_params_ for next encoded frame.
  FillPictureControlParams(options);

  bool used_as_ref = picture_params_.RefreshFrameFlags != 0;
  input_arguments_.PictureControlDesc.Flags =
      used_as_ref
          ? D3D12_VIDEO_ENCODER_PICTURE_CONTROL_FLAG_USED_AS_REFERENCE_PICTURE
          : D3D12_VIDEO_ENCODER_PICTURE_CONTROL_FLAG_NONE;
  auto reconstructed_buffer = dpb_.GetCurrentFrame();
  D3D12_VIDEO_ENCODE_REFERENCE_FRAMES reference_frames{};
  if (!IsKeyFrame()) {
    reference_frames = dpb_.ToD3D12VideoEncodeReferenceFrames();
  }
  input_arguments_.PictureControlDesc.ReferenceFrames = reference_frames;
  input_arguments_.pInputFrame = input_frame;
  input_arguments_.InputFrameSubresource = input_frame_subresource;
  D3D12_VIDEO_ENCODER_RECONSTRUCTED_PICTURE reconstructed_picture = {
      .pReconstructedPicture =
          used_as_ref ? reconstructed_buffer.resource_ : nullptr,
      .ReconstructedPictureSubresource =
          used_as_ref ? reconstructed_buffer.subresource_ : 0,
  };

  if (EncoderStatus result = video_encoder_wrapper_->Encode(
          input_arguments_, reconstructed_picture);
      !result.is_ok()) {
    return result;
  }

  // For now we only update sequence header for Rec.601 and Rec.709 on key
  // frames.
  if (IsKeyFrame()) {
    sequence_header_.color_range =
        input_color_space.GetRangeID() == gfx::ColorSpace::RangeID::FULL
            ? kLibgav1ColorRangeFull
            : kLibgav1ColorRangeStudio;
    if (IsRec601(input_color_space)) {
      sequence_header_.color_primaries = kLibgav1ColorPrimaryBt601;
      sequence_header_.transfer_characteristics =
          kLibgav1TransferCharacteristicsBt601;
      sequence_header_.matrix_coefficients = kLibgav1MatrixCoefficientsBt601;
      sequence_header_.color_description_present_flag = true;
    } else if (IsRec709(input_color_space)) {
      sequence_header_.color_primaries = kLibgav1ColorPrimaryBt709;
      sequence_header_.transfer_characteristics =
          kLibgav1TransferCharacteristicsBt709;
      sequence_header_.matrix_coefficients = kLibgav1MatrixCoefficientsBt709;
      sequence_header_.color_description_present_flag = true;
    }
  }

  return EncoderStatus::Codes::kOk;
}

EncoderStatus::Or<size_t>
D3D12VideoEncodeAV1Delegate::GetEncodedBitstreamWrittenBytesCount(
    const ScopedD3D12ResourceMap& metadata) {
  if (metadata.data().size() <
      sizeof(D3D12_VIDEO_ENCODER_OUTPUT_METADATA) +
          sizeof(D3D12_VIDEO_ENCODER_FRAME_SUBREGION_METADATA) +
          sizeof(
              D3D12_VIDEO_ENCODER_AV1_PICTURE_CONTROL_SUBREGIONS_LAYOUT_DATA_TILES) +
          sizeof(D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES)) {
    return EncoderStatus::Codes::kEncoderHardwareDriverError;
  }

  size_t compressed_size =
      reinterpret_cast<const D3D12_VIDEO_ENCODER_OUTPUT_METADATA*>(
          metadata.data().data())
          ->EncodedBitstreamWrittenBytesCount;
  auto subregions =
      reinterpret_cast<const D3D12_VIDEO_ENCODER_OUTPUT_METADATA*>(
          metadata.data().data())
          ->WrittenSubregionsCount;

  // We always enable full frame encoding, so there should be only one
  // subregion.
  if (subregions != 1) {
    return {EncoderStatus::Codes::kEncoderHardwareDriverError,
            "D3D12VideoEncodeAV1Delegate: unexpected number of subregions."};
  }

  size_t suregion_size =
      reinterpret_cast<const D3D12_VIDEO_ENCODER_FRAME_SUBREGION_METADATA*>(
          metadata.data()
              .subspan(sizeof(D3D12_VIDEO_ENCODER_OUTPUT_METADATA))
              .data())
          ->bSize;
  // Some Intel drivers may return incorrect EncodedBitstreamWrittenBytesCount,
  // so use subregion size as the authoritative one when they differ.
  if (suregion_size != 0 && suregion_size != compressed_size) {
    compressed_size = suregion_size;
  }
  return compressed_size;
}

void D3D12VideoEncodeAV1Delegate::RefreshDPBAndDescriptors() {
  if (svc_layers_) {
    svc_layers_->PostEncode(picture_params_.RefreshFrameFlags);
  }

  if (picture_params_.RefreshFrameFlags == 0) {
    return;
  }

  uint8_t refreshed_dpb_idx =
      std::countr_zero((picture_params_.RefreshFrameFlags));
  CHECK_LT(refreshed_dpb_idx, max_num_ref_frames_);
  dpb_.ReplaceWithCurrentFrame(refreshed_dpb_idx);

  // Follow the RefreshFrameFlags to update the descriptors array.
  D3D12_VIDEO_ENCODER_AV1_REFERENCE_PICTURE_DESCRIPTOR a_descriptor = {
      .ReconstructedPictureResourceIndex = static_cast<UINT>(refreshed_dpb_idx),
      .TemporalLayerIndexPlus1 = picture_params_.TemporalLayerIndexPlus1,
      .SpatialLayerIndexPlus1 = picture_params_.SpatialLayerIndexPlus1,
      .FrameType = picture_params_.FrameType,
      .OrderHint = picture_params_.OrderHint,
      .PictureIndex = picture_params_.PictureIndex};
  for (size_t i = 0; i < reference_descriptors_.size(); i++) {
    if (picture_params_.RefreshFrameFlags & (1 << i)) {
      reference_descriptors_[i] = a_descriptor;
    }
  }
}

size_t D3D12VideoEncodeAV1Delegate::PackAV1BitstreamHeader(
    const AV1BitstreamBuilder::FrameHeader& frame_header,
    size_t compressed_size,
    base::span<uint8_t> bitstream_buffer) {
  AV1BitstreamBuilder pack_header;
  uint8_t temporal_idx =
      metadata_.svc_generic ? metadata_.svc_generic->temporal_idx : 0;
  bool has_extension = GetNumTemporalLayers() > 1;
  // See section 5.6 of the AV1 specification.
  pack_header.WriteOBUHeader(
      /*type=*/libgav1::kObuTemporalDelimiter,
      /*has_size=*/true, has_extension && !IsKeyFrame(), temporal_idx);
  pack_header.WriteValueInLeb128(0);
  if (IsKeyFrame()) {
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
  pack_header.WriteOBUHeader(/*type=*/libgav1::kObuFrame, /*has_size=*/true,
                             has_extension, temporal_idx);
  AV1BitstreamBuilder frame_obu =
      AV1BitstreamBuilder::BuildFrameHeaderOBU(sequence_header_, frame_header);
  CHECK_EQ(frame_obu.OutstandingBits() % 8, 0ull);
  pack_header.WriteValueInLeb128(frame_obu.OutstandingBits() / 8 +
                                 compressed_size);
  pack_header.AppendBitstreamBuffer(std::move(frame_obu));

  std::vector<uint8_t> packed_frame_header = std::move(pack_header).Flush();
  size_t packed_header_size = packed_frame_header.size();
  bitstream_buffer.first(packed_header_size)
      .copy_from(base::span(packed_frame_header));

  return packed_header_size;
}

EncoderStatus::Or<size_t> D3D12VideoEncodeAV1Delegate::ReadbackBitstream(
    base::span<uint8_t> bitstream_buffer) {
  auto metadata_or_error = video_encoder_wrapper_->GetEncoderOutputMetadata();
  if (!metadata_or_error.has_value()) {
    return std::move(metadata_or_error).error();
  }
  ScopedD3D12ResourceMap metadata = std::move(metadata_or_error).value();
  auto compressed_size_or_size = GetEncodedBitstreamWrittenBytesCount(metadata);
  if (!compressed_size_or_size.has_value()) {
    return std::move(compressed_size_or_size).error();
  }
  size_t compressed_size = std::move(compressed_size_or_size).value();
  DVLOG(4) << base::StringPrintf("%s: compressed_size = %lu", __func__,
                                 compressed_size);

  size_t post_encode_values_offset =
      sizeof(D3D12_VIDEO_ENCODER_OUTPUT_METADATA) +
      1 /*subregions*/ * sizeof(D3D12_VIDEO_ENCODER_FRAME_SUBREGION_METADATA) +
      sizeof(
          D3D12_VIDEO_ENCODER_AV1_PICTURE_CONTROL_SUBREGIONS_LAYOUT_DATA_TILES);

  if (metadata.data().size() <
      post_encode_values_offset +
          sizeof(D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES)) {
    return {EncoderStatus::Codes::kEncoderHardwareDriverError,
            "D3D12VideoEncodeAV1Delegate: metadata buffer is too small."};
  }

  // SAFETY: The post_encode_values is guaranteed by above check to be within
  // the size of metadata.data().
  UNSAFE_BUFFERS(
      const D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES& post_encode_values =
          *reinterpret_cast<const D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES*>(
              metadata.data().subspan(post_encode_values_offset).data()));

  DVLOG(4) << PrintPostEncodeValues(post_encode_values);

  auto frame_header = FillAV1BuilderFrameHeader(picture_ctrl_, picture_params_,
                                                sequence_header_);
  if (!UpdateFrameHeaderPostEncode(enc_caps_.post_value_flags,
                                   post_encode_values, frame_header)) {
    return {EncoderStatus::Codes::kEncoderHardwareDriverError,
            "D3D12VideoEncodeAV1Delegate: invalid post encode values."};
  }

  D3D12_RANGE written_range{};
  metadata.Commit(&written_range);

  size_t packed_header_size =
      PackAV1BitstreamHeader(frame_header, compressed_size, bitstream_buffer);
  auto size_or_error = D3D12VideoEncodeDelegate::ReadbackBitstream(
      bitstream_buffer.subspan(packed_header_size));
  if (!size_or_error.has_value()) {
    return std::move(size_or_error).error();
  }

  // Notify SW BRC about recent encoded frame size.
  if (software_brc_) {
    software_brc_->PostEncodeUpdate(packed_header_size + compressed_size);
  }

  RefreshDPBAndDescriptors();

  metadata_.key_frame = IsKeyFrame();
  metadata_.qp = frame_header.base_qindex;
  return packed_header_size + compressed_size;
}

// D3D12 video drivers may use AV1 encoding parameters that are different from
// those submitted by the client. Whenever the driver does this, it sets
// corresponding bit masks in `post_encode_flags` and fills `post_encode_values`
// with the parameters that were actually used for encoding. This function
// updates the frame header with the values from `post_encode_values` if that
// happens.
bool D3D12VideoEncodeAV1Delegate::UpdateFrameHeaderPostEncode(
    const D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAGS& post_encode_flags,
    const D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES& post_encode_values,
    AV1BitstreamBuilder::FrameHeader& frame_header) {
  if (post_encode_flags ==
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_NONE) {
    return true;
  }

  if (post_encode_flags &
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_CDEF_DATA) {
    const auto& cdef = post_encode_values.CDEF;
    if ((1u << cdef.CdefBits) > std::size(cdef.CdefYPriStrength) ||
        cdef.CdefDampingMinus3 > 3) {
      LOG(ERROR) << "Invalid CDEF params in output metadata.";
      return false;
    }
    frame_header.cdef_damping_minus_3 = cdef.CdefDampingMinus3;
    frame_header.cdef_bits = cdef.CdefBits;
    for (uint32_t i = 0; i < (1 << cdef.CdefBits); i++) {
      frame_header.cdef_y_pri_strength[i] =
          base::span(cdef.CdefYPriStrength)[i] & 0xf;
      auto cdef_y_sec_strength = base::span(cdef.CdefYSecStrength)[i];
      // AV1 spec section 5.9.19.
      if (cdef_y_sec_strength == 4) {
        frame_header.cdef_y_sec_strength[i] = 3;
      } else {
        frame_header.cdef_y_sec_strength[i] = cdef_y_sec_strength & 0x3;
      }
      frame_header.cdef_uv_pri_strength[i] =
          base::span(cdef.CdefUVPriStrength)[i] & 0xf;
      auto cdef_uv_sec_strength = base::span(cdef.CdefUVSecStrength)[i];
      if (cdef_uv_sec_strength == 4) {
        frame_header.cdef_uv_sec_strength[i] = 3;
      } else {
        frame_header.cdef_uv_sec_strength[i] = cdef_uv_sec_strength & 0x3;
      }
    }
  }

  if (post_encode_flags &
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_LOOP_FILTER) {
    const auto& loop_filter = post_encode_values.LoopFilter;
    frame_header.filter_level[0] =
        base::span(loop_filter.LoopFilterLevel)[0] & 0x3f;
    frame_header.filter_level[1] =
        base::span(loop_filter.LoopFilterLevel)[1] & 0x3f;
    frame_header.filter_level_u = loop_filter.LoopFilterLevelU & 0x3f;
    frame_header.filter_level_v = loop_filter.LoopFilterLevelV & 0x3f;
    frame_header.sharpness_level = loop_filter.LoopFilterSharpnessLevel & 0x7;
    frame_header.loop_filter_delta_enabled =
        loop_filter.LoopFilterDeltaEnabled & 0x1;
    if (frame_header.loop_filter_delta_enabled) {
      frame_header.update_ref_delta = loop_filter.UpdateRefDelta & 0x1;
      frame_header.update_mode_delta = loop_filter.UpdateModeDelta & 0x1;
    } else {
      frame_header.update_ref_delta = false;
      frame_header.update_mode_delta = false;
    }
    frame_header.loop_filter_delta_update =
        frame_header.update_ref_delta | frame_header.update_mode_delta;
    if (frame_header.update_ref_delta) {
      for (uint32_t i = 0; i < std::size(loop_filter.RefDeltas); i++) {
        frame_header.loop_filter_ref_deltas[i] =
            base::saturated_cast<int8_t>(base::span(loop_filter.RefDeltas)[i]);
      }
    }
    if (frame_header.update_mode_delta) {
      for (uint32_t i = 0; i < std::size(loop_filter.ModeDeltas); i++) {
        frame_header.loop_filter_mode_deltas[i] =
            base::saturated_cast<int8_t>(base::span(loop_filter.ModeDeltas)[i]);
      }
    }
  }

  if (post_encode_flags &
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_LOOP_FILTER_DELTA) {
    const auto& loop_filter_delta = post_encode_values.LoopFilterDelta;
    frame_header.delta_lf_present = loop_filter_delta.DeltaLFPresent & 0x1;
    frame_header.delta_lf_res = loop_filter_delta.DeltaLFMulti & 0x3;
    frame_header.delta_lf_multi = loop_filter_delta.DeltaLFRes & 0x1;
  }

  if (post_encode_flags &
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_QUANTIZATION) {
    const auto& quantization = post_encode_values.Quantization;
    frame_header.base_qindex =
        base::saturated_cast<uint32_t>(quantization.BaseQIndex);
    frame_header.delta_q_y_dc =
        base::saturated_cast<int8_t>(quantization.YDCDeltaQ);
    frame_header.delta_q_u_dc =
        base::saturated_cast<int8_t>(quantization.UDCDeltaQ);
    frame_header.delta_q_u_ac =
        base::saturated_cast<int8_t>(quantization.UACDeltaQ);
    frame_header.delta_q_v_dc =
        base::saturated_cast<int8_t>(quantization.VDCDeltaQ);
    frame_header.delta_q_v_ac =
        base::saturated_cast<int8_t>(quantization.VACDeltaQ);
    frame_header.using_qmatrix = quantization.UsingQMatrix & 0x1;
    frame_header.qm_y = base::saturated_cast<uint8_t>(quantization.QMY);
    frame_header.qm_u = base::saturated_cast<uint8_t>(quantization.QMU);
    frame_header.qm_v = base::saturated_cast<uint8_t>(quantization.QMV);
  }

  if (post_encode_flags &
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_QUANTIZATION_DELTA) {
    const auto& quantization_delta = post_encode_values.QuantizationDelta;
    frame_header.delta_q_present = quantization_delta.DeltaQPresent & 0x1;
    frame_header.delta_q_res = quantization_delta.DeltaQRes & 0x3;
  }

  const auto& segmentation = post_encode_values.SegmentationConfig;
  if (auto num_segments = segmentation.NumSegments) {
    if (num_segments > std::size(segmentation.SegmentsData)) {
      LOG(ERROR) << "Invalid number of segments in output metadata: "
                 << num_segments;
      return false;
    }
    frame_header.segment_number = num_segments;
    frame_header.segmentation_enabled = true;
    frame_header.segmentation_update_map = segmentation.UpdateMap & 0x1;
    frame_header.segmentation_temporal_update =
        segmentation.TemporalUpdate & 0x1;
    frame_header.segmentation_update_data = segmentation.UpdateData;
    for (uint32_t i = 0; i < std::size(segmentation.SegmentsData); i++) {
      uint32_t enabled =
          base::span(segmentation.SegmentsData)[i].EnabledFeatures & 0xFF;
      if (enabled) {
        // SEG_LVL_ALT_Q's segmentation mode flag is 0x10, so we need to left
        // shift 1 bit.
        for (uint32_t j = 0; j < 8; ++j) {
          frame_header.feature_enabled[i][j] = (enabled & (1u << (j + 1))) != 0;
        }
        for (uint32_t j = 0;
             j <
             std::size(base::span(segmentation.SegmentsData)[i].FeatureValue);
             j++) {
          if (enabled & (1u << j)) {
            frame_header.feature_data[j][i] =
                base::saturated_cast<int16_t>(base::span(
                    base::span(segmentation.SegmentsData)[i].FeatureValue)[j]);
          }
        }
      }
    }
  } else {
    frame_header.segmentation_enabled = false;
  }

  if (post_encode_flags &
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_PRIMARY_REF_FRAME) {
    frame_header.primary_ref_frame =
        base::saturated_cast<uint8_t>(post_encode_values.PrimaryRefFrame);
  }

  if (post_encode_flags &
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_REFERENCE_INDICES) {
    for (uint32_t i = 0; i < std::size(post_encode_values.ReferenceIndices);
         i++) {
      frame_header.ref_frame_idx[i] = base::saturated_cast<uint8_t>(
          base::span(post_encode_values.ReferenceIndices)[i]);
    }
  }

  if (post_encode_flags &
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_COMPOUND_PREDICTION_MODE) {
    frame_header.reference_select =
        post_encode_values.CompoundPredictionType & 0x1;
  }

  return true;
}

}  // namespace media
