// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_video_encode_delegate.h"

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "build/buildflag.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "media/base/bitrate.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/video_codecs.h"
#include "media/base/video_encoder.h"
#include "media/base/video_types.h"
#include "media/base/win/d3d12_mocks.h"
#include "media/base/win/d3d12_video_mocks.h"
#include "media/gpu/windows/d3d12_video_encode_av1_delegate.h"
#include "media/gpu/windows/d3d12_video_encode_h264_delegate.h"
#include "media/gpu/windows/d3d12_video_helpers.h"
#include "media/gpu/windows/format_utils.h"
#include "media/media_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/microsoft_dxheaders/src/include/directx/d3dx12_core.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
#include "media/gpu/windows/d3d12_video_encode_h265_delegate.h"
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

namespace media {

namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

constexpr uint32_t kMinFrameSize = 16;
constexpr uint32_t kMaxFrameSize = 1920;
constexpr size_t kMinPayloadSize = 1024;
constexpr size_t kMaxPayloadSize = 4096;

struct Av1CodecSupportConfig {
  uint32_t supported_interpolation_filters =
      1u << D3D12_VIDEO_ENCODER_AV1_INTERPOLATION_FILTERS_EIGHTTAP;
  D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAGS supported_feature_flags =
      D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_NONE;
  D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAGS required_feature_flags =
      D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_NONE;
  std::array<uint32_t, 2> supported_tx_modes = {
      D3D12_VIDEO_ENCODER_AV1_TX_MODE_FLAG_SELECT,
      D3D12_VIDEO_ENCODER_AV1_TX_MODE_FLAG_SELECT,
  };
  std::array<std::array<D3D12_VIDEO_ENCODER_AV1_RESTORATION_SUPPORT_FLAGS, 3>,
             3>
      supported_restoration_params = {};
  D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAGS post_encode_flags =
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_NONE;
};

struct HevcCodecSupportConfig {
  D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAGS support_flags =
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_NONE;
  D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_CUSIZE min_luma_cu_size =
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_CUSIZE_8x8;
  D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_CUSIZE max_luma_cu_size =
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_CUSIZE_64x64;
  D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_TUSIZE min_luma_tu_size =
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_TUSIZE_4x4;
  D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_TUSIZE max_luma_tu_size =
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_TUSIZE_32x32;
  uint8_t max_transform_hierarchy_depth_inter = 0;
  uint8_t max_transform_hierarchy_depth_intra = 0;
};

struct HevcEncoderSupportConfig {
  D3D12_VIDEO_ENCODER_SUPPORT_FLAGS support_flags =
      D3D12_VIDEO_ENCODER_SUPPORT_FLAG_GENERAL_SUPPORT_OK;
  D3D12_VIDEO_ENCODER_VALIDATION_FLAGS validation_flags =
      D3D12_VIDEO_ENCODER_VALIDATION_FLAG_NONE;
  D3D12_VIDEO_ENCODER_PROFILE_HEVC suggested_profile =
      D3D12_VIDEO_ENCODER_PROFILE_HEVC_MAIN;
  D3D12_VIDEO_ENCODER_LEVELS_HEVC suggested_level =
      D3D12_VIDEO_ENCODER_LEVELS_HEVC_31;
  D3D12_VIDEO_ENCODER_TIER_HEVC suggested_tier =
      D3D12_VIDEO_ENCODER_TIER_HEVC_MAIN;
  uint32_t subregion_block_pixels_size = 16;
};

struct H264PictureControlSupportConfig {
  uint8_t max_long_term_references = 0;
  uint8_t max_dpb_capacity = 0;
};

struct H264CodecConfigurationSupportConfig {
  D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_H264_FLAGS support_flags =
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_H264_FLAG_NONE;
  D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_H264_SLICES_DEBLOCKING_MODE_FLAGS
  deblocking_modes =
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_H264_SLICES_DEBLOCKING_MODE_FLAG_NONE;
};

Av1CodecSupportConfig ConsumeAv1CodecSupportConfig(
    FuzzedDataProvider& provider) {
  Av1CodecSupportConfig config;
  config.supported_interpolation_filters =
      provider.ConsumeIntegralInRange<uint32_t>(
          1u << D3D12_VIDEO_ENCODER_AV1_INTERPOLATION_FILTERS_EIGHTTAP,
          (1u << D3D12_VIDEO_ENCODER_AV1_INTERPOLATION_FILTERS_SWITCHABLE));
  for (auto& mode : config.supported_tx_modes) {
    mode = provider.ConsumeBool()
               ? D3D12_VIDEO_ENCODER_AV1_TX_MODE_FLAG_SELECT
               : D3D12_VIDEO_ENCODER_AV1_TX_MODE_FLAG_LARGEST;
  }
  const D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAGS base_features =
      static_cast<D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAGS>(
          D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_CDEF_FILTERING |
          D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_ORDER_HINT_TOOLS |
          D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_LOOP_RESTORATION_FILTER |
          D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_PALETTE_ENCODING |
          D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_INTRA_BLOCK_COPY);
  if (provider.ConsumeBool()) {
    config.required_feature_flags =
        provider.ConsumeBool()
            ? base_features
            : static_cast<D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAGS>(base_features &
                                                                 ~1u);
  }
  if (provider.ConsumeBool()) {
    config.supported_feature_flags = base_features;
  } else if (config.required_feature_flags !=
             D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_NONE) {
    config.supported_feature_flags =
        provider.ConsumeBool() ? config.required_feature_flags
                               : D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_NONE;
  }
  for (auto& type_params : config.supported_restoration_params) {
    for (auto& plane_params : type_params) {
      uint32_t mask = 0;
      if (provider.ConsumeBool()) {
        mask |= D3D12_VIDEO_ENCODER_AV1_RESTORATION_SUPPORT_FLAG_256x256;
      }
      if (provider.ConsumeBool()) {
        mask |= D3D12_VIDEO_ENCODER_AV1_RESTORATION_SUPPORT_FLAG_128x128;
      }
      if (provider.ConsumeBool()) {
        mask |= D3D12_VIDEO_ENCODER_AV1_RESTORATION_SUPPORT_FLAG_64x64;
      }
      if (provider.ConsumeBool()) {
        mask |= D3D12_VIDEO_ENCODER_AV1_RESTORATION_SUPPORT_FLAG_32x32;
      }
      plane_params =
          static_cast<D3D12_VIDEO_ENCODER_AV1_RESTORATION_SUPPORT_FLAGS>(mask);
    }
  }
  static constexpr std::array kPostEncodeFlags = {
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_QUANTIZATION,
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_QUANTIZATION_DELTA,
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_LOOP_FILTER,
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_LOOP_FILTER_DELTA,
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_CDEF_DATA,
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_CONTEXT_UPDATE_TILE_ID,
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_COMPOUND_PREDICTION_MODE,
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_PRIMARY_REF_FRAME,
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_REFERENCE_INDICES,
  };
  for (const auto& flag : kPostEncodeFlags) {
    if (provider.ConsumeBool()) {
      config.post_encode_flags |= flag;
    }
  }
  return config;
}

HevcCodecSupportConfig ConsumeHevcCodecSupportConfig(
    FuzzedDataProvider& provider) {
  HevcCodecSupportConfig config;
  static constexpr std::array kCuSizes = {
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_CUSIZE_8x8,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_CUSIZE_16x16,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_CUSIZE_32x32,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_CUSIZE_64x64,
  };
  static constexpr std::array kTuSizes = {
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_TUSIZE_4x4,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_TUSIZE_8x8,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_TUSIZE_16x16,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_TUSIZE_32x32,
  };
  auto min_cu = provider.PickValueInArray(kCuSizes);
  auto max_cu = provider.PickValueInArray(kCuSizes);
  if (min_cu > max_cu) {
    std::swap(min_cu, max_cu);
  }
  config.min_luma_cu_size = min_cu;
  config.max_luma_cu_size = max_cu;

  auto min_tu = provider.PickValueInArray(kTuSizes);
  auto max_tu = provider.PickValueInArray(kTuSizes);
  if (min_tu > max_tu) {
    std::swap(min_tu, max_tu);
  }
  config.min_luma_tu_size = min_tu;
  config.max_luma_tu_size = max_tu;

  config.max_transform_hierarchy_depth_inter =
      provider.ConsumeIntegral<uint8_t>();
  config.max_transform_hierarchy_depth_intra =
      provider.ConsumeIntegral<uint8_t>();

  static constexpr std::array kSupportFlags = {
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_BFRAME_LTR_COMBINED_SUPPORT,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_INTRA_SLICE_CONSTRAINED_ENCODING_SUPPORT,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_CONSTRAINED_INTRAPREDICTION_SUPPORT,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_SAO_FILTER_SUPPORT,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_ASYMETRIC_MOTION_PARTITION_SUPPORT,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_ASYMETRIC_MOTION_PARTITION_REQUIRED,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_TRANSFORM_SKIP_SUPPORT,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_DISABLING_LOOP_FILTER_ACROSS_SLICES_SUPPORT,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_P_FRAMES_IMPLEMENTED_AS_LOW_DELAY_B_FRAMES,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_NUM_REF_IDX_ACTIVE_OVERRIDE_FLAG_SLICE_SUPPORT,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_TRANSFORM_SKIP_ROTATION_ENABLED_SUPPORT,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_TRANSFORM_SKIP_ROTATION_ENABLED_REQUIRED,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_TRANSFORM_SKIP_CONTEXT_ENABLED_SUPPORT,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_TRANSFORM_SKIP_CONTEXT_ENABLED_REQUIRED,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_IMPLICIT_RDPCM_ENABLED_SUPPORT,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_IMPLICIT_RDPCM_ENABLED_REQUIRED,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_EXPLICIT_RDPCM_ENABLED_SUPPORT,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_EXPLICIT_RDPCM_ENABLED_REQUIRED,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_EXTENDED_PRECISION_PROCESSING_SUPPORT,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_EXTENDED_PRECISION_PROCESSING_REQUIRED,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_INTRA_SMOOTHING_DISABLED_SUPPORT,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_INTRA_SMOOTHING_DISABLED_REQUIRED,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_HIGH_PRECISION_OFFSETS_ENABLED_SUPPORT,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_HIGH_PRECISION_OFFSETS_ENABLED_REQUIRED,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_PERSISTENT_RICE_ADAPTATION_ENABLED_SUPPORT,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_PERSISTENT_RICE_ADAPTATION_ENABLED_REQUIRED,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_CABAC_BYPASS_ALIGNMENT_ENABLED_SUPPORT,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_CABAC_BYPASS_ALIGNMENT_ENABLED_REQUIRED,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_CROSS_COMPONENT_PREDICTION_ENABLED_FLAG_SUPPORT,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_CROSS_COMPONENT_PREDICTION_ENABLED_FLAG_REQUIRED,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_CHROMA_QP_OFFSET_LIST_ENABLED_FLAG_SUPPORT,
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_CHROMA_QP_OFFSET_LIST_ENABLED_FLAG_REQUIRED,
  };
  for (auto flag : kSupportFlags) {
    if (provider.ConsumeBool()) {
      config.support_flags |= flag;
    }
  }

  return config;
}

HevcEncoderSupportConfig ConsumeHevcEncoderSupportConfig(
    FuzzedDataProvider& provider) {
  HevcEncoderSupportConfig config;
  config.support_flags = D3D12_VIDEO_ENCODER_SUPPORT_FLAG_GENERAL_SUPPORT_OK;
  if (provider.ConsumeBool()) {
    config.support_flags |=
        D3D12_VIDEO_ENCODER_SUPPORT_FLAG_RATE_CONTROL_RECONFIGURATION_AVAILABLE;
  }
  if (provider.ConsumeBool()) {
    config.support_flags |=
        D3D12_VIDEO_ENCODER_SUPPORT_FLAG_RESOLUTION_RECONFIGURATION_AVAILABLE;
  }
  if (provider.ConsumeBool()) {
    config.support_flags |=
        D3D12_VIDEO_ENCODER_SUPPORT_FLAG_RATE_CONTROL_VBV_SIZE_CONFIG_AVAILABLE;
  }
  if (provider.ConsumeBool()) {
    config.support_flags |=
        D3D12_VIDEO_ENCODER_SUPPORT_FLAG_RATE_CONTROL_FRAME_ANALYSIS_AVAILABLE;
  }
  if (provider.ConsumeBool()) {
    config.support_flags |=
        D3D12_VIDEO_ENCODER_SUPPORT_FLAG_RECONSTRUCTED_FRAMES_REQUIRE_TEXTURE_ARRAYS;
  }
  if (provider.ConsumeBool()) {
    config.support_flags |=
        D3D12_VIDEO_ENCODER_SUPPORT_FLAG_RATE_CONTROL_DELTA_QP_AVAILABLE;
  }
  if (provider.ConsumeBool()) {
    config.support_flags |=
        D3D12_VIDEO_ENCODER_SUPPORT_FLAG_SUBREGION_LAYOUT_RECONFIGURATION_AVAILABLE;
  }
  if (provider.ConsumeBool()) {
    config.support_flags |=
        D3D12_VIDEO_ENCODER_SUPPORT_FLAG_RATE_CONTROL_ADJUSTABLE_QP_RANGE_AVAILABLE;
  }
  if (provider.ConsumeBool()) {
    config.support_flags |=
        D3D12_VIDEO_ENCODER_SUPPORT_FLAG_RATE_CONTROL_INITIAL_QP_AVAILABLE;
  }
  if (provider.ConsumeBool()) {
    config.support_flags |=
        D3D12_VIDEO_ENCODER_SUPPORT_FLAG_RATE_CONTROL_MAX_FRAME_SIZE_AVAILABLE;
  }
  if (provider.ConsumeBool()) {
    config.support_flags |=
        D3D12_VIDEO_ENCODER_SUPPORT_FLAG_SEQUENCE_GOP_RECONFIGURATION_AVAILABLE;
  }
  if (provider.ConsumeBool()) {
    config.support_flags |=
        D3D12_VIDEO_ENCODER_SUPPORT_FLAG_MOTION_ESTIMATION_PRECISION_MODE_LIMIT_AVAILABLE;
  }
  if (provider.ConsumeBool()) {
    config.support_flags |=
        D3D12_VIDEO_ENCODER_SUPPORT_FLAG_RATE_CONTROL_EXTENSION1_SUPPORT;
  }
  if (provider.ConsumeBool()) {
    config.support_flags |=
        D3D12_VIDEO_ENCODER_SUPPORT_FLAG_RATE_CONTROL_QUALITY_VS_SPEED_AVAILABLE;
  }
  if (provider.ConsumeBool()) {
    config.support_flags |=
        D3D12_VIDEO_ENCODER_SUPPORT_FLAG_READABLE_RECONSTRUCTED_PICTURE_LAYOUT_AVAILABLE;
  }
  config.suggested_profile = provider.ConsumeBool()
                                 ? D3D12_VIDEO_ENCODER_PROFILE_HEVC_MAIN
                                 : D3D12_VIDEO_ENCODER_PROFILE_HEVC_MAIN10;
  config.suggested_level = provider.ConsumeBool()
                               ? D3D12_VIDEO_ENCODER_LEVELS_HEVC_1
                               : D3D12_VIDEO_ENCODER_LEVELS_HEVC_31;
  config.subregion_block_pixels_size =
      provider.ConsumeIntegralInRange<uint32_t>(
          std::numeric_limits<uint32_t>::min(),
          std::numeric_limits<uint32_t>::max());
  return config;
}

H264PictureControlSupportConfig ConsumeH264PictureControlSupportConfig(
    FuzzedDataProvider& provider) {
  H264PictureControlSupportConfig config;
  config.max_long_term_references = provider.ConsumeIntegral<uint8_t>();
  config.max_dpb_capacity = provider.ConsumeIntegral<uint8_t>();
  return config;
}

H264CodecConfigurationSupportConfig ConsumeH264CodecConfigurationSupportConfig(
    FuzzedDataProvider& provider) {
  H264CodecConfigurationSupportConfig config;
  if (provider.ConsumeBool()) {
    config.support_flags |=
        D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_H264_FLAG_CABAC_ENCODING_SUPPORT;
  }
  if (provider.ConsumeBool()) {
    config.support_flags |=
        D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_H264_FLAG_INTRA_SLICE_CONSTRAINED_ENCODING_SUPPORT;
  }
  if (provider.ConsumeBool()) {
    config.deblocking_modes |=
        D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_H264_SLICES_DEBLOCKING_MODE_FLAG_NONE;
  }
  if (provider.ConsumeBool()) {
    config.deblocking_modes |=
        D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_H264_SLICES_DEBLOCKING_MODE_FLAG_1_DISABLE_ALL_SLICE_BLOCK_EDGES;
  }
  if (provider.ConsumeBool()) {
    config.deblocking_modes |=
        D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_H264_SLICES_DEBLOCKING_MODE_FLAG_0_ALL_LUMA_CHROMA_SLICE_BLOCK_EDGES_ALWAYS_FILTERED;
  }
  if (provider.ConsumeBool()) {
    config.deblocking_modes |=
        D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_H264_SLICES_DEBLOCKING_MODE_FLAG_2_DISABLE_SLICE_BOUNDARIES_BLOCKS;
  }
  return config;
}

D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES ConsumeAv1PostEncodeValues(
    FuzzedDataProvider& provider) {
  D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES values = {};

  values.CDEF.CdefBits = provider.ConsumeIntegralInRange<uint64_t>(
      std::numeric_limits<uint64_t>::min(),
      std::numeric_limits<uint64_t>::max());
  values.CDEF.CdefDampingMinus3 = provider.ConsumeIntegralInRange<uint64_t>(
      std::numeric_limits<uint64_t>::min(),
      std::numeric_limits<uint64_t>::max());
  for (auto& value : values.CDEF.CdefYPriStrength) {
    value = provider.ConsumeIntegralInRange<uint64_t>(
        std::numeric_limits<uint64_t>::min(),
        std::numeric_limits<uint64_t>::max());
  }
  for (auto& value : values.CDEF.CdefYSecStrength) {
    value = provider.ConsumeIntegralInRange<uint64_t>(
        std::numeric_limits<uint64_t>::min(),
        std::numeric_limits<uint64_t>::max());
  }
  for (auto& value : values.CDEF.CdefUVPriStrength) {
    value = provider.ConsumeIntegralInRange<uint64_t>(
        std::numeric_limits<uint64_t>::min(),
        std::numeric_limits<uint64_t>::max());
  }
  for (auto& value : values.CDEF.CdefUVSecStrength) {
    value = provider.ConsumeIntegralInRange<uint64_t>(
        std::numeric_limits<uint64_t>::min(),
        std::numeric_limits<uint64_t>::max());
  }

  for (auto& level : values.LoopFilter.LoopFilterLevel) {
    level = provider.ConsumeIntegralInRange<uint64_t>(
        std::numeric_limits<uint64_t>::min(),
        std::numeric_limits<uint64_t>::max());
  }
  values.LoopFilter.LoopFilterLevelU =
      provider.ConsumeIntegralInRange<uint64_t>(
          std::numeric_limits<uint64_t>::min(),
          std::numeric_limits<uint64_t>::max());
  values.LoopFilter.LoopFilterLevelV =
      provider.ConsumeIntegralInRange<uint64_t>(
          std::numeric_limits<uint64_t>::min(),
          std::numeric_limits<uint64_t>::max());
  values.LoopFilter.LoopFilterSharpnessLevel =
      provider.ConsumeIntegralInRange<uint64_t>(
          std::numeric_limits<uint64_t>::min(),
          std::numeric_limits<uint64_t>::max());
  values.LoopFilter.LoopFilterDeltaEnabled =
      provider.ConsumeIntegralInRange<uint64_t>(
          std::numeric_limits<uint64_t>::min(),
          std::numeric_limits<uint64_t>::max());
  values.LoopFilter.UpdateRefDelta = provider.ConsumeIntegralInRange<uint64_t>(
      std::numeric_limits<uint64_t>::min(),
      std::numeric_limits<uint64_t>::max());
  values.LoopFilter.UpdateModeDelta = provider.ConsumeIntegralInRange<uint64_t>(
      std::numeric_limits<uint64_t>::min(),
      std::numeric_limits<uint64_t>::max());
  for (auto& delta : values.LoopFilter.RefDeltas) {
    delta = provider.ConsumeIntegralInRange<int64_t>(
        std::numeric_limits<int64_t>::min(),
        std::numeric_limits<int64_t>::max());
  }
  for (auto& delta : values.LoopFilter.ModeDeltas) {
    delta = provider.ConsumeIntegralInRange<int64_t>(
        std::numeric_limits<int64_t>::min(),
        std::numeric_limits<int64_t>::max());
  }

  values.LoopFilterDelta.DeltaLFPresent =
      provider.ConsumeIntegralInRange<uint64_t>(
          std::numeric_limits<uint64_t>::min(),
          std::numeric_limits<uint64_t>::max());
  values.LoopFilterDelta.DeltaLFMulti =
      provider.ConsumeIntegralInRange<uint64_t>(
          std::numeric_limits<uint64_t>::min(),
          std::numeric_limits<uint64_t>::max());
  values.LoopFilterDelta.DeltaLFRes = provider.ConsumeIntegralInRange<uint64_t>(
      std::numeric_limits<uint64_t>::min(),
      std::numeric_limits<uint64_t>::max());

  values.Quantization.BaseQIndex = provider.ConsumeIntegralInRange<uint64_t>(
      std::numeric_limits<uint64_t>::min(),
      std::numeric_limits<uint64_t>::max());
  values.Quantization.YDCDeltaQ = provider.ConsumeIntegralInRange<int64_t>(
      std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max());
  values.Quantization.UDCDeltaQ = provider.ConsumeIntegralInRange<int64_t>(
      std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max());
  values.Quantization.UACDeltaQ = provider.ConsumeIntegralInRange<int64_t>(
      std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max());
  values.Quantization.VDCDeltaQ = provider.ConsumeIntegralInRange<int64_t>(
      std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max());
  values.Quantization.VACDeltaQ = provider.ConsumeIntegralInRange<int64_t>(
      std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max());
  values.Quantization.UsingQMatrix = provider.ConsumeIntegralInRange<uint64_t>(
      std::numeric_limits<uint64_t>::min(),
      std::numeric_limits<uint64_t>::max());
  values.Quantization.QMY = provider.ConsumeIntegralInRange<uint64_t>(
      std::numeric_limits<uint64_t>::min(),
      std::numeric_limits<uint64_t>::max());
  values.Quantization.QMU = provider.ConsumeIntegralInRange<uint64_t>(
      std::numeric_limits<uint64_t>::min(),
      std::numeric_limits<uint64_t>::max());
  values.Quantization.QMV = provider.ConsumeIntegralInRange<uint64_t>(
      std::numeric_limits<uint64_t>::min(),
      std::numeric_limits<uint64_t>::max());

  values.QuantizationDelta.DeltaQPresent =
      provider.ConsumeIntegralInRange<uint64_t>(
          std::numeric_limits<uint64_t>::min(),
          std::numeric_limits<uint64_t>::max());
  values.QuantizationDelta.DeltaQRes =
      provider.ConsumeIntegralInRange<uint64_t>(
          std::numeric_limits<uint64_t>::min(),
          std::numeric_limits<uint64_t>::max());

  if (provider.ConsumeBool()) {
    values.SegmentationConfig.NumSegments =
        provider.ConsumeIntegralInRange<uint64_t>(
            std::numeric_limits<uint64_t>::min(),
            std::numeric_limits<uint64_t>::max());
  } else {
    values.SegmentationConfig.NumSegments = 0;
  }
  values.SegmentationConfig.UpdateMap =
      provider.ConsumeIntegralInRange<uint64_t>(
          std::numeric_limits<uint64_t>::min(),
          std::numeric_limits<uint64_t>::max());
  values.SegmentationConfig.TemporalUpdate =
      provider.ConsumeIntegralInRange<uint64_t>(
          std::numeric_limits<uint64_t>::min(),
          std::numeric_limits<uint64_t>::max());
  values.SegmentationConfig.UpdateData =
      provider.ConsumeIntegralInRange<uint64_t>(
          std::numeric_limits<uint64_t>::min(),
          std::numeric_limits<uint64_t>::max());
  for (auto& segment : values.SegmentationConfig.SegmentsData) {
    segment.EnabledFeatures = provider.ConsumeIntegralInRange<uint64_t>(
        std::numeric_limits<uint64_t>::min(),
        std::numeric_limits<uint64_t>::max());
    for (auto& value : segment.FeatureValue) {
      value = provider.ConsumeIntegralInRange<int64_t>(
          std::numeric_limits<int64_t>::min(),
          std::numeric_limits<int64_t>::max());
    }
  }

  values.PrimaryRefFrame = provider.ConsumeIntegralInRange<uint64_t>(
      std::numeric_limits<uint64_t>::min(),
      std::numeric_limits<uint64_t>::max());
  for (auto& index : values.ReferenceIndices) {
    index = provider.ConsumeIntegralInRange<uint64_t>(
        std::numeric_limits<uint64_t>::min(),
        std::numeric_limits<uint64_t>::max());
  }
  values.CompoundPredictionType = provider.ConsumeIntegralInRange<uint64_t>(
      std::numeric_limits<uint64_t>::min(),
      std::numeric_limits<uint64_t>::max());

  return values;
}

class FuzzerVideoEncoderWrapper : public D3D12VideoEncoderWrapper {
 public:
  FuzzerVideoEncoderWrapper(
      size_t payload_size,
      D3D12_VIDEO_ENCODER_CODEC codec,
      std::optional<D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES>
          av1_post_encode_values)
      : D3D12VideoEncoderWrapper(nullptr, nullptr),
        payload_size_(payload_size),
        metadata_resource_(MakeComPtr<NiceMock<D3D12ResourceMock>>()) {
    size_t metadata_size = sizeof(D3D12_VIDEO_ENCODER_OUTPUT_METADATA);
    if (codec == D3D12_VIDEO_ENCODER_CODEC_AV1) {
      metadata_size +=
          sizeof(D3D12_VIDEO_ENCODER_FRAME_SUBREGION_METADATA) +
          sizeof(
              D3D12_VIDEO_ENCODER_AV1_PICTURE_CONTROL_SUBREGIONS_LAYOUT_DATA_TILES) +
          sizeof(D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES);
    }
    metadata_bytes_.resize(metadata_size);
    D3D12_VIDEO_ENCODER_OUTPUT_METADATA metadata = {};
    metadata.EncodedBitstreamWrittenBytesCount = payload_size_;
    metadata.WrittenSubregionsCount = 1;
    base::span(metadata_bytes_)
        .first(sizeof(metadata))
        .copy_from(base::byte_span_from_ref(metadata));

    if (codec == D3D12_VIDEO_ENCODER_CODEC_AV1) {
      size_t offset = sizeof(D3D12_VIDEO_ENCODER_OUTPUT_METADATA);
      D3D12_VIDEO_ENCODER_FRAME_SUBREGION_METADATA subregion = {};
      subregion.bSize = static_cast<UINT64>(payload_size_);
      base::span(metadata_bytes_)
          .subspan(offset, sizeof(subregion))
          .copy_from(base::byte_span_from_ref(subregion));
      offset += sizeof(D3D12_VIDEO_ENCODER_FRAME_SUBREGION_METADATA);

      D3D12_VIDEO_ENCODER_AV1_PICTURE_CONTROL_SUBREGIONS_LAYOUT_DATA_TILES
      tiles = {};
      base::span(metadata_bytes_)
          .subspan(offset, sizeof(tiles))
          .copy_from(base::byte_span_from_ref(tiles));
      offset += sizeof(
          D3D12_VIDEO_ENCODER_AV1_PICTURE_CONTROL_SUBREGIONS_LAYOUT_DATA_TILES);

      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES post_encode_values = {};
      if (av1_post_encode_values.has_value()) {
        post_encode_values = av1_post_encode_values.value();
      }
      base::span(metadata_bytes_)
          .subspan(offset, sizeof(post_encode_values))
          .copy_from(base::byte_span_from_ref(post_encode_values));
    }

    D3D12_RESOURCE_DESC metadata_desc = CD3DX12_RESOURCE_DESC::Buffer(
        static_cast<UINT64>(metadata_bytes_.size()));
    ON_CALL(*metadata_resource_.Get(), GetDesc)
        .WillByDefault(Return(metadata_desc));
    ON_CALL(*metadata_resource_.Get(), Map)
        .WillByDefault([this](UINT, const D3D12_RANGE*, void** data) {
          *data = metadata_bytes_.data();
          return S_OK;
        });
    ON_CALL(*metadata_resource_.Get(), Unmap)
        .WillByDefault([](UINT, const D3D12_RANGE*) {});
  }

  bool Initialize(uint32_t) override { return true; }
  bool Wait(D3D12FenceAndValue) override { return true; }
  EncoderStatus Encode(
      const D3D12_VIDEO_ENCODER_ENCODEFRAME_INPUT_ARGUMENTS&,
      const D3D12_VIDEO_ENCODER_RECONSTRUCTED_PICTURE&) override {
    return EncoderStatus::Codes::kOk;
  }

  EncoderStatus::Or<ScopedD3D12ResourceMap> GetEncoderOutputMetadata()
      const override {
    ScopedD3D12ResourceMap map;
    if (!map.Map(metadata_resource_.Get(), 0, nullptr)) {
      return EncoderStatus::Codes::kEncoderInitializationError;
    }
    return map;
  }

  EncoderStatus ReadbackBitstream(base::span<uint8_t> data) const override {
    std::fill(data.begin(), data.end(), 0);
    return EncoderStatus::Codes::kOk;
  }

 private:
  size_t payload_size_ = 0;
  mutable std::vector<uint8_t> metadata_bytes_;
  Microsoft::WRL::ComPtr<D3D12ResourceMock> metadata_resource_;
};

class FuzzerVideoProcessorWrapper : public D3D12VideoProcessorWrapper {
 public:
  explicit FuzzerVideoProcessorWrapper(
      Microsoft::WRL::ComPtr<ID3D12VideoDevice> device)
      : D3D12VideoProcessorWrapper(std::move(device)),
        fence_(MakeComPtr<NiceMock<D3D12FenceMock>>()) {}

  bool Init() override { return true; }
  bool Wait(D3D12FenceAndValue) override { return true; }

  D3D12FenceAndValue ProcessFrames(ID3D12Resource*,
                                   UINT,
                                   const gfx::ColorSpace&,
                                   const gfx::Rect&,
                                   ID3D12Resource*,
                                   UINT,
                                   const gfx::ColorSpace&,
                                   const gfx::Rect&) override {
    return {fence_, 1};
  }

 private:
  Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
};

std::unique_ptr<D3D12VideoEncoderWrapper> CreateFuzzerVideoEncoderWrapper(
    size_t payload_size,
    ID3D12VideoDevice*,
    D3D12_VIDEO_ENCODER_CODEC codec,
    const D3D12_VIDEO_ENCODER_PROFILE_DESC&,
    const D3D12_VIDEO_ENCODER_LEVEL_SETTING&,
    DXGI_FORMAT,
    const D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION&,
    const D3D12_VIDEO_ENCODER_PICTURE_RESOLUTION_DESC&) {
  return std::make_unique<FuzzerVideoEncoderWrapper>(payload_size, codec,
                                                     std::nullopt);
}

std::unique_ptr<D3D12VideoEncoderWrapper> CreateFuzzerVideoEncoderWrapperAV1(
    size_t payload_size,
    D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES post_encode_values,
    ID3D12VideoDevice*,
    D3D12_VIDEO_ENCODER_CODEC codec,
    const D3D12_VIDEO_ENCODER_PROFILE_DESC&,
    const D3D12_VIDEO_ENCODER_LEVEL_SETTING&,
    DXGI_FORMAT,
    const D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION&,
    const D3D12_VIDEO_ENCODER_PICTURE_RESOLUTION_DESC&) {
  return std::make_unique<FuzzerVideoEncoderWrapper>(payload_size, codec,
                                                     post_encode_values);
}

std::unique_ptr<D3D12VideoProcessorWrapper> CreateFuzzerVideoProcessorWrapper(
    Microsoft::WRL::ComPtr<ID3D12VideoDevice>&& video_device) {
  return std::make_unique<FuzzerVideoProcessorWrapper>(std::move(video_device));
}

gfx::Size ConsumeFrameSize(FuzzedDataProvider& provider) {
  uint32_t width =
      provider.ConsumeIntegralInRange<uint32_t>(kMinFrameSize, kMaxFrameSize);
  uint32_t height =
      provider.ConsumeIntegralInRange<uint32_t>(kMinFrameSize, kMaxFrameSize);
  // 4:2:0 subsampling requires frame size to be even.
  width &= ~1u;
  height &= ~1u;
  width = std::max(width, kMinFrameSize);
  height = std::max(height, kMinFrameSize);
  return gfx::Size(width, height);
}

Microsoft::WRL::ComPtr<D3D12ResourceMock> CreateInputResource(
    const gfx::Size& size,
    DXGI_FORMAT format) {
  auto resource = MakeComPtr<NiceMock<D3D12ResourceMock>>();
  D3D12_RESOURCE_DESC desc =
      CD3DX12_RESOURCE_DESC::Tex2D(format, size.width(), size.height(), 1, 1);
  ON_CALL(*resource.Get(), GetDesc).WillByDefault(Return(desc));
  return resource;
}

void ConfigureDeviceCommon(
    Microsoft::WRL::ComPtr<D3D12DeviceMock> device,
    Microsoft::WRL::ComPtr<D3D12VideoDevice3Mock> video_device) {
  ON_CALL(*video_device.Get(), QueryInterface(IID_ID3D12Device, _))
      .WillByDefault(SetComPointeeAndReturnOk<1>(device.Get()));
  ON_CALL(*video_device.Get(), QueryInterface(IID_ID3D12VideoDevice1, _))
      .WillByDefault(SetComPointeeAndReturnOk<1>(video_device.Get()));

  ON_CALL(*device.Get(), CreateCommittedResource)
      .WillByDefault([](const D3D12_HEAP_PROPERTIES*, D3D12_HEAP_FLAGS,
                        const D3D12_RESOURCE_DESC* desc, D3D12_RESOURCE_STATES,
                        const D3D12_CLEAR_VALUE*, REFIID, void** ppv) {
        auto resource = MakeComPtr<NiceMock<D3D12ResourceMock>>();
        D3D12_RESOURCE_DESC copy = *desc;
        ON_CALL(*resource.Get(), GetDesc).WillByDefault(Return(copy));
        resource->AddRef();
        *ppv = resource.Get();
        return S_OK;
      });
}

void ConfigureVideoDeviceForH264(
    D3D12VideoDevice3Mock* video_device,
    const H264PictureControlSupportConfig& picture_support_config,
    const H264CodecConfigurationSupportConfig& codec_support_config) {
  ON_CALL(*video_device, CheckFeatureSupport)
      .WillByDefault([picture_support_config, codec_support_config](
                         D3D12_FEATURE_VIDEO feature, void* data, UINT) {
        switch (feature) {
          case D3D12_FEATURE_VIDEO_ENCODER_CODEC: {
            auto* codec =
                static_cast<D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC*>(data);
            codec->IsSupported = codec->Codec == D3D12_VIDEO_ENCODER_CODEC_H264;
            return S_OK;
          }
          case D3D12_FEATURE_VIDEO_ENCODER_PROFILE_LEVEL: {
            auto* profile_level =
                static_cast<D3D12_FEATURE_DATA_VIDEO_ENCODER_PROFILE_LEVEL*>(
                    data);
            profile_level->IsSupported = true;
            if (profile_level->MinSupportedLevel.pH264LevelSetting) {
              *profile_level->MinSupportedLevel.pH264LevelSetting =
                  D3D12_VIDEO_ENCODER_LEVELS_H264_1;
            }
            if (profile_level->MaxSupportedLevel.pH264LevelSetting) {
              *profile_level->MaxSupportedLevel.pH264LevelSetting =
                  D3D12_VIDEO_ENCODER_LEVELS_H264_31;
            }
            return S_OK;
          }
          case D3D12_FEATURE_VIDEO_ENCODER_INPUT_FORMAT: {
            auto* input_format =
                static_cast<D3D12_FEATURE_DATA_VIDEO_ENCODER_INPUT_FORMAT*>(
                    data);
            input_format->IsSupported = true;
            return S_OK;
          }
          case D3D12_FEATURE_VIDEO_ENCODER_CODEC_PICTURE_CONTROL_SUPPORT: {
            auto* picture_control = static_cast<
                D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC_PICTURE_CONTROL_SUPPORT*>(
                data);
            picture_control->IsSupported = true;
            if (picture_control->PictureSupport.pH264Support) {
              picture_control->PictureSupport.pH264Support
                  ->MaxLongTermReferences =
                  picture_support_config.max_long_term_references;
              picture_control->PictureSupport.pH264Support->MaxDPBCapacity =
                  picture_support_config.max_dpb_capacity;
            }
            return S_OK;
          }
          case D3D12_FEATURE_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT: {
            auto* config = static_cast<
                D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT*>(
                data);
            config->IsSupported = true;
            if (config->CodecSupportLimits.pH264Support) {
              config->CodecSupportLimits.pH264Support->SupportFlags =
                  codec_support_config.support_flags;
              config->CodecSupportLimits.pH264Support
                  ->DisableDeblockingFilterSupportedModes =
                  codec_support_config.deblocking_modes;
            }
            return S_OK;
          }
          case D3D12_FEATURE_VIDEO_ENCODER_SUPPORT: {
            auto* support =
                static_cast<D3D12_FEATURE_DATA_VIDEO_ENCODER_SUPPORT*>(data);
            support->SupportFlags =
                D3D12_VIDEO_ENCODER_SUPPORT_FLAG_GENERAL_SUPPORT_OK;
            support->ValidationFlags = D3D12_VIDEO_ENCODER_VALIDATION_FLAG_NONE;
            if (support->SuggestedProfile.pH264Profile) {
              *support->SuggestedProfile.pH264Profile =
                  D3D12_VIDEO_ENCODER_PROFILE_H264_MAIN;
            }
            if (support->SuggestedLevel.pH264LevelSetting) {
              *support->SuggestedLevel.pH264LevelSetting =
                  D3D12_VIDEO_ENCODER_LEVELS_H264_31;
            }
            return S_OK;
          }
          case D3D12_FEATURE_VIDEO_ENCODER_RATE_CONTROL_MODE: {
            auto* rate_control = static_cast<
                D3D12_FEATURE_DATA_VIDEO_ENCODER_RATE_CONTROL_MODE*>(data);
            rate_control->IsSupported = true;
            return S_OK;
          }
          default:
            return E_INVALIDARG;
        }
      });
}

void ConfigureVideoDeviceForAV1(D3D12VideoDevice3Mock* video_device,
                                const Av1CodecSupportConfig& support_config) {
  ON_CALL(*video_device, CheckFeatureSupport)
      .WillByDefault([support_config](D3D12_FEATURE_VIDEO feature, void* data,
                                      UINT) {
        switch (feature) {
          case D3D12_FEATURE_VIDEO_ENCODER_CODEC: {
            auto* codec =
                static_cast<D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC*>(data);
            codec->IsSupported = codec->Codec == D3D12_VIDEO_ENCODER_CODEC_AV1;
            return S_OK;
          }
          case D3D12_FEATURE_VIDEO_ENCODER_PROFILE_LEVEL: {
            auto* profile_level =
                static_cast<D3D12_FEATURE_DATA_VIDEO_ENCODER_PROFILE_LEVEL*>(
                    data);
            profile_level->IsSupported = true;
            return S_OK;
          }
          case D3D12_FEATURE_VIDEO_ENCODER_INPUT_FORMAT: {
            auto* input_format =
                static_cast<D3D12_FEATURE_DATA_VIDEO_ENCODER_INPUT_FORMAT*>(
                    data);
            input_format->IsSupported = true;
            return S_OK;
          }
          case D3D12_FEATURE_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT: {
            auto* config_support = static_cast<
                D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT*>(
                data);
            config_support->IsSupported = true;
            if (config_support->CodecSupportLimits.pAV1Support) {
              auto* av1 = config_support->CodecSupportLimits.pAV1Support;
              av1->SupportedInterpolationFilters = static_cast<
                  D3D12_VIDEO_ENCODER_AV1_INTERPOLATION_FILTERS_FLAGS>(
                  support_config.supported_interpolation_filters);
              av1->SupportedFeatureFlags =
                  support_config.supported_feature_flags;
              av1->RequiredFeatureFlags = support_config.required_feature_flags;
              av1->PostEncodeValuesFlags = support_config.post_encode_flags;
              const size_t tx_mode_count =
                  std::min(std::size(av1->SupportedTxModes),
                           support_config.supported_tx_modes.size());
              auto src_tx_modes = base::span(support_config.supported_tx_modes);
              auto src_tx_mode_it = src_tx_modes.begin();
              size_t tx_index = 0;
              for (auto& mode : av1->SupportedTxModes) {
                if (tx_index < tx_mode_count) {
                  mode = static_cast<D3D12_VIDEO_ENCODER_AV1_TX_MODE_FLAGS>(
                      *src_tx_mode_it);
                  ++src_tx_mode_it;
                } else {
                  mode = D3D12_VIDEO_ENCODER_AV1_TX_MODE_FLAG_SELECT;
                }
                ++tx_index;
              }
              auto src_restoration =
                  base::span(support_config.supported_restoration_params);
              auto src_restoration_it = src_restoration.begin();
              for (auto& type_params : av1->SupportedRestorationParams) {
                auto src_plane_it = src_restoration_it->begin();
                for (auto& plane_params : type_params) {
                  plane_params = *src_plane_it;
                  ++src_plane_it;
                }
                ++src_restoration_it;
              }
            }
            return S_OK;
          }
          case D3D12_FEATURE_VIDEO_ENCODER_SUPPORT1: {
            auto* support =
                static_cast<D3D12_FEATURE_DATA_VIDEO_ENCODER_SUPPORT1*>(data);
            support->SupportFlags =
                D3D12_VIDEO_ENCODER_SUPPORT_FLAG_GENERAL_SUPPORT_OK;
            support->ValidationFlags = D3D12_VIDEO_ENCODER_VALIDATION_FLAG_NONE;
            if (support->SuggestedProfile.pAV1Profile) {
              *support->SuggestedProfile.pAV1Profile =
                  D3D12_VIDEO_ENCODER_AV1_PROFILE_MAIN;
            }
            if (support->SuggestedLevel.pAV1LevelSetting) {
              support->SuggestedLevel.pAV1LevelSetting->Level =
                  D3D12_VIDEO_ENCODER_AV1_LEVELS_3_1;
              support->SuggestedLevel.pAV1LevelSetting->Tier =
                  D3D12_VIDEO_ENCODER_AV1_TIER_MAIN;
            }
            if (support->pResolutionDependentSupport) {
              support->pResolutionDependentSupport[0].SubregionBlockPixelsSize =
                  16;
            }
            return S_OK;
          }
          case D3D12_FEATURE_VIDEO_ENCODER_RATE_CONTROL_MODE: {
            auto* rate_control = static_cast<
                D3D12_FEATURE_DATA_VIDEO_ENCODER_RATE_CONTROL_MODE*>(data);
            rate_control->IsSupported = true;
            return S_OK;
          }
          default:
            return E_INVALIDARG;
        }
      });
}

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
void ConfigureVideoDeviceForH265(
    D3D12VideoDevice3Mock* video_device,
    const HevcCodecSupportConfig& support_config,
    const HevcEncoderSupportConfig& encoder_config) {
  ON_CALL(*video_device, CheckFeatureSupport)
      .WillByDefault([support_config, encoder_config](
                         D3D12_FEATURE_VIDEO feature, void* data, UINT) {
        switch (feature) {
          case D3D12_FEATURE_VIDEO_ENCODER_CODEC: {
            auto* codec =
                static_cast<D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC*>(data);
            codec->IsSupported = codec->Codec == D3D12_VIDEO_ENCODER_CODEC_HEVC;
            return S_OK;
          }
          case D3D12_FEATURE_VIDEO_ENCODER_PROFILE_LEVEL: {
            auto* profile_level =
                static_cast<D3D12_FEATURE_DATA_VIDEO_ENCODER_PROFILE_LEVEL*>(
                    data);
            profile_level->IsSupported = true;
            if (profile_level->MinSupportedLevel.pHEVCLevelSetting) {
              *profile_level->MinSupportedLevel.pHEVCLevelSetting = {
                  D3D12_VIDEO_ENCODER_LEVELS_HEVC_1,
                  D3D12_VIDEO_ENCODER_TIER_HEVC_MAIN};
            }
            if (profile_level->MaxSupportedLevel.pHEVCLevelSetting) {
              *profile_level->MaxSupportedLevel.pHEVCLevelSetting = {
                  D3D12_VIDEO_ENCODER_LEVELS_HEVC_31,
                  D3D12_VIDEO_ENCODER_TIER_HEVC_MAIN};
            }
            return S_OK;
          }
          case D3D12_FEATURE_VIDEO_ENCODER_INPUT_FORMAT: {
            auto* input_format =
                static_cast<D3D12_FEATURE_DATA_VIDEO_ENCODER_INPUT_FORMAT*>(
                    data);
            input_format->IsSupported = true;
            return S_OK;
          }
          case D3D12_FEATURE_VIDEO_ENCODER_CODEC_PICTURE_CONTROL_SUPPORT: {
            auto* picture_control = static_cast<
                D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC_PICTURE_CONTROL_SUPPORT*>(
                data);
            picture_control->IsSupported = true;
            if (picture_control->PictureSupport.pHEVCSupport) {
              picture_control->PictureSupport.pHEVCSupport
                  ->MaxLongTermReferences = 1;
              picture_control->PictureSupport.pHEVCSupport->MaxDPBCapacity = 16;
            }
            return S_OK;
          }
          case D3D12_FEATURE_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT: {
            auto* config = static_cast<
                D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT*>(
                data);
            config->IsSupported = true;
            if (config->CodecSupportLimits.pHEVCSupport) {
              *config->CodecSupportLimits.pHEVCSupport = {
                  .SupportFlags = support_config.support_flags,
                  .MinLumaCodingUnitSize = support_config.min_luma_cu_size,
                  .MaxLumaCodingUnitSize = support_config.max_luma_cu_size,
                  .MinLumaTransformUnitSize = support_config.min_luma_tu_size,
                  .MaxLumaTransformUnitSize = support_config.max_luma_tu_size,
                  .max_transform_hierarchy_depth_inter =
                      support_config.max_transform_hierarchy_depth_inter,
                  .max_transform_hierarchy_depth_intra =
                      support_config.max_transform_hierarchy_depth_intra,
              };
            }
            return S_OK;
          }
          case D3D12_FEATURE_VIDEO_ENCODER_SUPPORT: {
            auto* support =
                static_cast<D3D12_FEATURE_DATA_VIDEO_ENCODER_SUPPORT*>(data);
            support->SupportFlags = encoder_config.support_flags;
            support->ValidationFlags = encoder_config.validation_flags;
            if (support->SuggestedProfile.pHEVCProfile) {
              *support->SuggestedProfile.pHEVCProfile =
                  encoder_config.suggested_profile;
            }
            if (support->SuggestedLevel.pHEVCLevelSetting) {
              *support->SuggestedLevel.pHEVCLevelSetting = {
                  encoder_config.suggested_level,
                  encoder_config.suggested_tier};
            }
            if (support->pResolutionDependentSupport) {
              support->pResolutionDependentSupport[0].SubregionBlockPixelsSize =
                  encoder_config.subregion_block_pixels_size;
            }
            return S_OK;
          }
          case D3D12_FEATURE_VIDEO_ENCODER_RATE_CONTROL_MODE: {
            auto* rate_control = static_cast<
                D3D12_FEATURE_DATA_VIDEO_ENCODER_RATE_CONTROL_MODE*>(data);
            rate_control->IsSupported = true;
            return S_OK;
          }
          default:
            return E_INVALIDARG;
        }
      });
}
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

VideoEncodeAccelerator::Config BuildConfig(VideoCodecProfile profile,
                                           VideoPixelFormat format,
                                           const gfx::Size& size,
                                           uint32_t bitrate,
                                           uint32_t framerate,
                                           bool is_screen) {
  VideoEncodeAccelerator::Config config;
  config.input_format = format;
  config.input_visible_size = size;
  config.output_profile = profile;
  config.bitrate = Bitrate::ConstantBitrate(bitrate);
  config.framerate = framerate;
  config.storage_type = VideoEncodeAccelerator::Config::StorageType::kShmem;
  config.content_type =
      is_screen ? VideoEncodeAccelerator::Config::ContentType::kDisplay
                : VideoEncodeAccelerator::Config::ContentType::kCamera;
  config.manual_reference_buffer_control = is_screen;
  config.gop_length = framerate * 2;
  return config;
}

}  // namespace

int RunD3D12VideoEncodeDelegateFuzzer(FuzzedDataProvider& provider) {
  auto device = MakeComPtr<NiceMock<D3D12DeviceMock>>();
  auto video_device = MakeComPtr<NiceMock<D3D12VideoDevice3Mock>>();
  ConfigureDeviceCommon(device, video_device);

  size_t payload_size =
      provider.ConsumeIntegralInRange<size_t>(kMinPayloadSize, kMaxPayloadSize);
  auto processor_factory =
      base::BindRepeating(&CreateFuzzerVideoProcessorWrapper);

  gfx::Size frame_size = ConsumeFrameSize(provider);
  uint32_t bitrate = provider.ConsumeIntegralInRange<uint32_t>(10000, 2000000);
  uint32_t framerate = provider.ConsumeIntegralInRange<uint32_t>(1, 60);
  bool is_screen = provider.ConsumeBool();

  enum class CodecChoice { kH264, kH265, kAV1 };
  CodecChoice codec =
      provider.ConsumeBool() ? CodecChoice::kH264 : CodecChoice::kAV1;
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  if (provider.ConsumeBool()) {
    codec = CodecChoice::kH265;
  }
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

  std::unique_ptr<D3D12VideoEncodeDelegate> delegate;
  VideoEncodeAccelerator::Config config;
  VideoPixelFormat input_format = PIXEL_FORMAT_NV12;

  switch (codec) {
    case CodecChoice::kH264: {
      auto h264_picture_support_config =
          ConsumeH264PictureControlSupportConfig(provider);
      auto h264_codec_support_config =
          ConsumeH264CodecConfigurationSupportConfig(provider);
      ConfigureVideoDeviceForH264(video_device.Get(),
                                  h264_picture_support_config,
                                  h264_codec_support_config);
      bool use_high10 = provider.ConsumeBool();
      VideoCodecProfile profile =
          use_high10 ? H264PROFILE_HIGH10PROFILE : H264PROFILE_MAIN;
      input_format = use_high10 ? PIXEL_FORMAT_P010LE : PIXEL_FORMAT_NV12;
      config = BuildConfig(profile, input_format, frame_size, bitrate,
                           framerate, is_screen);
      delegate = std::make_unique<D3D12VideoEncodeH264Delegate>(
          video_device, gpu::GpuDriverBugWorkarounds{});
      auto encoder_factory =
          base::BindRepeating(&CreateFuzzerVideoEncoderWrapper, payload_size);
      delegate->SetFactoriesForTesting(encoder_factory, processor_factory);
      break;
    }
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    case CodecChoice::kH265: {
      auto hevc_support_config = ConsumeHevcCodecSupportConfig(provider);
      auto hevc_encoder_support_config =
          ConsumeHevcEncoderSupportConfig(provider);
      ConfigureVideoDeviceForH265(video_device.Get(), hevc_support_config,
                                  hevc_encoder_support_config);
      bool use_main10 = provider.ConsumeBool();
      VideoCodecProfile profile =
          use_main10 ? HEVCPROFILE_MAIN10 : HEVCPROFILE_MAIN;
      input_format = use_main10 ? PIXEL_FORMAT_P010LE : PIXEL_FORMAT_NV12;
      config = BuildConfig(profile, input_format, frame_size, bitrate,
                           framerate, is_screen);
      delegate = std::make_unique<D3D12VideoEncodeH265Delegate>(
          video_device, gpu::GpuDriverBugWorkarounds{});
      auto encoder_factory =
          base::BindRepeating(&CreateFuzzerVideoEncoderWrapper, payload_size);
      delegate->SetFactoriesForTesting(encoder_factory, processor_factory);
      break;
    }
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    case CodecChoice::kAV1: {
      auto av1_support_config = ConsumeAv1CodecSupportConfig(provider);
      ConfigureVideoDeviceForAV1(video_device.Get(), av1_support_config);
      input_format = PIXEL_FORMAT_NV12;
      config = BuildConfig(AV1PROFILE_PROFILE_MAIN, input_format, frame_size,
                           bitrate, framerate, is_screen);
      delegate = std::make_unique<D3D12VideoEncodeAV1Delegate>(
          video_device, gpu::GpuDriverBugWorkarounds{});
      auto post_encode_values = ConsumeAv1PostEncodeValues(provider);
      auto encoder_factory =
          base::BindRepeating(&CreateFuzzerVideoEncoderWrapperAV1, payload_size,
                              post_encode_values);
      delegate->SetFactoriesForTesting(encoder_factory, processor_factory);
      break;
    }
  }

  if (!delegate->Initialize(config).is_ok()) {
    return 0;
  }

  DXGI_FORMAT dxgi_format = VideoPixelFormatToDxgiFormat(input_format);
  auto input_resource = CreateInputResource(frame_size, dxgi_format);
  D3D12PictureBuffer picture_buffer(input_resource, 0, {});

  auto shared_memory = base::UnsafeSharedMemoryRegion::Create(payload_size);
  BitstreamBuffer bitstream_buffer(provider.ConsumeIntegral<int32_t>(),
                                   shared_memory.Duplicate(), payload_size);

  VideoEncoder::EncodeOptions options(provider.ConsumeBool());
  if (provider.ConsumeBool()) {
    options.quantizer = provider.ConsumeIntegralInRange<int>(1, 51);
  }
  if (provider.ConsumeBool()) {
    size_t max_refs = delegate->GetMaxNumOfManualRefBuffers();
    if (max_refs > 0) {
      size_t ref_count = provider.ConsumeIntegralInRange<size_t>(0, max_refs);
      for (size_t i = 0; i < ref_count; ++i) {
        options.reference_buffers.push_back(
            static_cast<uint8_t>(provider.ConsumeIntegralInRange<int>(
                0, static_cast<int>(max_refs - 1))));
      }
      if (provider.ConsumeBool()) {
        options.update_buffer = static_cast<uint8_t>(
            provider.ConsumeIntegralInRange<int>(0, max_refs - 1));
      }
    }
  }

  gfx::ColorSpace color_space = gfx::ColorSpace::CreateREC709();
  delegate->Encode(picture_buffer, color_space, bitstream_buffer, options);

  return 0;
}

}  // namespace media

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);
  return media::RunD3D12VideoEncodeDelegateFuzzer(provider);
}
