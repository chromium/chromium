// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vp9_svc_layers.h"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <vector>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "media/gpu/vp9_picture.h"
#include "media/gpu/vp9_reference_frame_vector.h"
#include "media/parsers/vp9_parser.h"
#include "media/video/video_encode_accelerator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

constexpr gfx::Size kDefaultEncodeSize(1280, 720);
constexpr int kSpatialLayerResolutionDenom[] = {4, 2, 1};

gfx::Size GetDefaultSVCResolution(size_t spatial_index) {
  const int denom = kSpatialLayerResolutionDenom[spatial_index];
  return gfx::Size(kDefaultEncodeSize.width() / denom,
                   kDefaultEncodeSize.height() / denom);
}

std::vector<gfx::Size> GetDefaultSVCResolutions(size_t num_spatial_layers) {
  std::vector<gfx::Size> spatial_layer_resolutions(num_spatial_layers);
  for (size_t i = 0; i < num_spatial_layers; ++i) {
    spatial_layer_resolutions[i] = GetDefaultSVCResolution(i);
  }
  return spatial_layer_resolutions;
}

VP9SVCLayers::Config GetDefaultSVCLayers2Config(
    size_t num_spatial_layers,
    size_t num_temporal_layers,
    SVCInterLayerPredMode inter_layer_pred) {
  const auto& spatial_layer_resolutions =
      GetDefaultSVCResolutions(num_spatial_layers);
  return VP9SVCLayers::Config(spatial_layer_resolutions,
                              /*begin_active_layer=*/0,
                              spatial_layer_resolutions.size(),
                              num_temporal_layers, inter_layer_pred);
}

uint8_t GetTemporalIndex(size_t num_temporal_layers, size_t frame_num) {
  constexpr uint8_t kTemporalIndices[][4] = {
      {0, 0, 0, 0},
      {0, 1, 0, 1},
      {0, 2, 1, 2},
  };
  CHECK(1 <= num_temporal_layers && num_temporal_layers <= 3);
  return kTemporalIndices[num_temporal_layers - 1][frame_num % 4];
}

struct Vp9MetadataAndFrameNum {
  constexpr static size_t kInvalidFrameNum = std::numeric_limits<size_t>::max();

  Vp9MetadataAndFrameNum() : frame_num(kInvalidFrameNum) {}
  Vp9MetadataAndFrameNum(size_t frame_num, const Vp9Metadata& metadata)
      : frame_num(frame_num), metadata(metadata) {}

  bool is_valid() const { return frame_num != kInvalidFrameNum; }

  size_t frame_num = 0;
  Vp9Metadata metadata;
};

void VerifyReferenceFrame(const Vp9Metadata& metadata,
                          size_t frame_num,
                          const Vp9MetadataAndFrameNum& ref_frame,
                          SVCInterLayerPredMode inter_layer_pred) {
  ASSERT_TRUE(ref_frame.is_valid());
  const Vp9Metadata& ref_metadata = ref_frame.metadata;
  const uint8_t ref_spatial_index = ref_metadata.spatial_idx;
  // In key picture, upper spatial layers must refer the lower spatial layer.
  // Or referenced frames must be in the same spatial layer.
  if (frame_num == 0 && inter_layer_pred == SVCInterLayerPredMode::kOnKeyPic) {
    EXPECT_EQ(ref_spatial_index, metadata.spatial_idx - 1);
    EXPECT_TRUE(metadata.p_diffs.empty());
  } else {
    EXPECT_EQ(ref_spatial_index, metadata.spatial_idx);

    const std::vector<uint8_t> expected_p_diffs = {
        base::checked_cast<uint8_t>(frame_num - ref_frame.frame_num)};
    EXPECT_EQ(metadata.p_diffs, expected_p_diffs);
  }

  const size_t ref_temporal_index = ref_metadata.temporal_idx;
  EXPECT_LE(ref_temporal_index, metadata.temporal_idx);

  EXPECT_EQ(metadata.temporal_up_switch, ref_metadata.temporal_idx == 0);
}

void VerifykSVCFrame(
    const VP9SVCLayers::PictureParam& picture_param,
    const Vp9Metadata& metadata,
    const std::array<Vp9MetadataAndFrameNum, kVp9NumRefFrames>& ref_frames,
    size_t frame_num,
    size_t num_spatial_layers,
    size_t expected_begin_active_layer,
    size_t expected_end_active_layer) {
  const uint8_t temporal_index = metadata.temporal_idx;
  const uint8_t spatial_index = metadata.spatial_idx;

  EXPECT_EQ(picture_param.key_frame, frame_num == 0 && spatial_index == 0);
  EXPECT_EQ(metadata.end_of_picture, spatial_index == num_spatial_layers - 1);

  if (picture_param.key_frame) {
    EXPECT_EQ(spatial_index, 0);
    EXPECT_EQ(temporal_index, 0);
    EXPECT_FALSE(metadata.inter_pic_predicted);
    EXPECT_TRUE(metadata.temporal_up_switch);
    EXPECT_EQ(metadata.referenced_by_upper_spatial_layers,
              num_spatial_layers > 1);
    EXPECT_FALSE(metadata.reference_lower_spatial_layers);
    EXPECT_EQ(metadata.spatial_layer_resolutions,
              GetDefaultSVCResolutions(num_spatial_layers));
    EXPECT_EQ(metadata.begin_active_spatial_layer_index,
              expected_begin_active_layer);
    EXPECT_EQ(metadata.end_active_spatial_layer_index,
              expected_end_active_layer);
    EXPECT_TRUE(metadata.p_diffs.empty());

    EXPECT_EQ(picture_param.frame_size, GetDefaultSVCResolution(spatial_index));
    EXPECT_EQ(picture_param.refresh_frame_flags, 0xff);
    EXPECT_TRUE(picture_param.reference_frame_indices.empty());
    return;
  }

  EXPECT_EQ(picture_param.frame_size, GetDefaultSVCResolution(spatial_index));

  // Six slots at most in the reference pool are used in spatial/temporal layer
  // encoding. Additionally, non-keyframe must reference some frames.
  // |ref_frames_used| must be {true, false, false} because here is,
  // 1. if the frame is in key picture, it references one lower spatial layer,
  // 2. otherwise the frame doesn't reference other spatial layers and thus
  // references only one frame in the same spatial layer based on the current
  // reference pattern.
  EXPECT_EQ(picture_param.refresh_frame_flags & ~(0b111111u), 0u);
  ASSERT_EQ(picture_param.reference_frame_indices.size(), 1u);
  EXPECT_EQ(metadata.inter_pic_predicted, !metadata.p_diffs.empty());
  EXPECT_EQ(metadata.inter_pic_predicted, frame_num != 0);
  if (frame_num == 0) {
    EXPECT_TRUE(metadata.reference_lower_spatial_layers);
    EXPECT_EQ(metadata.referenced_by_upper_spatial_layers,
              spatial_index + 1 != num_spatial_layers);
  } else {
    EXPECT_FALSE(metadata.referenced_by_upper_spatial_layers);
    EXPECT_FALSE(metadata.reference_lower_spatial_layers);
  }

  EXPECT_TRUE(metadata.spatial_layer_resolutions.empty());

  // Check that the current frame doesn't reference upper layer frames.
  VerifyReferenceFrame(metadata, frame_num,
                       ref_frames[picture_param.reference_frame_indices[0]],
                       SVCInterLayerPredMode::kOnKeyPic);
}

void VerifySmodeFrame(
    const VP9SVCLayers::PictureParam& picture_param,
    const Vp9Metadata& metadata,
    const std::array<Vp9MetadataAndFrameNum, kVp9NumRefFrames>& ref_frames,
    size_t frame_num,
    size_t num_spatial_layers,
    size_t expected_begin_active_layer,
    size_t expected_end_active_layer) {
  const uint8_t temporal_index = metadata.temporal_idx;
  const uint8_t spatial_index = metadata.spatial_idx;
  EXPECT_EQ(picture_param.key_frame, frame_num == 0);
  EXPECT_EQ(metadata.end_of_picture, spatial_index == num_spatial_layers - 1);

  if (picture_param.key_frame) {
    EXPECT_EQ(temporal_index, 0u);
    EXPECT_FALSE(metadata.inter_pic_predicted);
    EXPECT_TRUE(metadata.temporal_up_switch);
    EXPECT_FALSE(metadata.referenced_by_upper_spatial_layers);
    EXPECT_FALSE(metadata.reference_lower_spatial_layers);
    EXPECT_EQ(metadata.spatial_layer_resolutions,
              GetDefaultSVCResolutions(num_spatial_layers));
    EXPECT_EQ(metadata.begin_active_spatial_layer_index,
              expected_begin_active_layer);
    EXPECT_EQ(metadata.end_active_spatial_layer_index,
              expected_end_active_layer);
    EXPECT_TRUE(metadata.p_diffs.empty());

    EXPECT_EQ(picture_param.frame_size, GetDefaultSVCResolution(spatial_index));
    if (spatial_index == 0) {
      EXPECT_EQ(picture_param.refresh_frame_flags, 0xff);
    } else {
      EXPECT_EQ(picture_param.refresh_frame_flags,
                (0x1 << (spatial_index * 2)));
    }
    EXPECT_TRUE(picture_param.reference_frame_indices.empty());
    return;
  }

  EXPECT_EQ(picture_param.frame_size, GetDefaultSVCResolution(spatial_index));
  EXPECT_EQ(picture_param.refresh_frame_flags & ~(0b111111u), 0u);
  EXPECT_EQ(picture_param.reference_frame_indices.size(), 1u);

  EXPECT_TRUE(metadata.inter_pic_predicted);
  EXPECT_FALSE(metadata.referenced_by_upper_spatial_layers);
  EXPECT_FALSE(metadata.reference_lower_spatial_layers);
  EXPECT_TRUE(metadata.spatial_layer_resolutions.empty());

  // Check that the current frame doesn't reference upper layer frames.
  VerifyReferenceFrame(metadata, frame_num,
                       ref_frames[picture_param.reference_frame_indices[0]],
                       SVCInterLayerPredMode::kOff);
}
}  // namespace

class VP9SVCLayersTest
    : public ::testing::TestWithParam<
          ::testing::tuple<size_t, size_t, SVCInterLayerPredMode>> {};

TEST_P(VP9SVCLayersTest, VerifyMetadata) {
  const size_t num_spatial_layers = ::testing::get<0>(GetParam());
  const size_t num_temporal_layers = ::testing::get<1>(GetParam());
  const SVCInterLayerPredMode inter_layer_pred_mode =
      ::testing::get<2>(GetParam());

  const VP9SVCLayers::Config config = GetDefaultSVCLayers2Config(
      num_spatial_layers, num_temporal_layers, inter_layer_pred_mode);
  VP9SVCLayers svc_layers(config);

  constexpr size_t kNumFramesToEncode = 100;
  std::array<Vp9MetadataAndFrameNum, kVp9NumRefFrames> ref_frames;
  constexpr size_t kKeyFrameInterval = 17;
  for (size_t i = 0; i < kNumFramesToEncode; ++i) {
    bool key_svc_frame = i % kKeyFrameInterval == 0;
    if (key_svc_frame) {
      svc_layers.Reset();
    }
    for (size_t sid = 0; sid < num_spatial_layers; ++sid) {
      bool key_frame = false;
      size_t frame_num = svc_layers.frame_num();
      if (frame_num == 0) {
        if (inter_layer_pred_mode == SVCInterLayerPredMode::kOnKeyPic) {
          key_frame = sid == 0;
        } else {
          key_frame = true;
        }
      }

      EXPECT_EQ(svc_layers.IsKeyFrame(), key_frame);
      VP9SVCLayers::PictureParam picture_param;
      Vp9Metadata metadata;
      svc_layers.GetPictureParamAndMetadata(picture_param, metadata);

      EXPECT_EQ(svc_layers.spatial_idx(), metadata.spatial_idx);
      EXPECT_EQ(svc_layers.spatial_idx(), sid);

      EXPECT_EQ(GetTemporalIndex(num_temporal_layers, frame_num),
                metadata.temporal_idx);

      if (inter_layer_pred_mode == SVCInterLayerPredMode::kOnKeyPic) {
        VerifykSVCFrame(picture_param, metadata, ref_frames, frame_num,
                        config.active_spatial_layer_resolutions.size(),
                        config.begin_active_layer, config.end_active_layer);
      } else {
        VerifySmodeFrame(picture_param, metadata, ref_frames, frame_num,
                         config.active_spatial_layer_resolutions.size(),
                         config.begin_active_layer, config.end_active_layer);
      }

      for (size_t j = 0; j < kVp9NumRefFrames; ++j) {
        if (picture_param.refresh_frame_flags & (1 << j)) {
          ref_frames[j] = Vp9MetadataAndFrameNum{frame_num, metadata};
        }
      }

      svc_layers.PostEncode(picture_param.refresh_frame_flags);
    }
  }
}

TEST_P(VP9SVCLayersTest, VerifyMetadataMultipleTimes) {
  const size_t num_spatial_layers = ::testing::get<0>(GetParam());
  const size_t num_temporal_layers = ::testing::get<1>(GetParam());
  const SVCInterLayerPredMode inter_layer_pred_mode =
      ::testing::get<2>(GetParam());

  const VP9SVCLayers::Config config = GetDefaultSVCLayers2Config(
      num_spatial_layers, num_temporal_layers, inter_layer_pred_mode);
  VP9SVCLayers svc_layers(config);

  constexpr size_t kNumFramesToEncode = 100;
  std::array<Vp9MetadataAndFrameNum, kVp9NumRefFrames> ref_frames;
  constexpr size_t kKeyFrameInterval = 17;
  constexpr size_t kReacquireInterval = 23;
  size_t frame_count = 0;
  for (size_t i = 0; i < kNumFramesToEncode; ++i) {
    bool key_svc_frame = i % kKeyFrameInterval == 0;
    if (key_svc_frame) {
      svc_layers.Reset();
    }
    for (size_t sid = 0; sid < num_spatial_layers; ++sid) {
      const bool reacquire = frame_count % kReacquireInterval;
      frame_count++;
      const size_t call_get_times = 1 + reacquire;
      for (size_t j = 0; j < call_get_times; j++) {
        bool key_frame = false;
        size_t frame_num = svc_layers.frame_num();
        if (frame_num == 0) {
          if (inter_layer_pred_mode == SVCInterLayerPredMode::kOnKeyPic) {
            key_frame = sid == 0;
          } else {
            key_frame = true;
          }
        }

        EXPECT_EQ(svc_layers.IsKeyFrame(), key_frame);
        VP9SVCLayers::PictureParam picture_param;
        Vp9Metadata metadata;
        svc_layers.GetPictureParamAndMetadata(picture_param, metadata);

        EXPECT_EQ(svc_layers.spatial_idx(), metadata.spatial_idx);
        EXPECT_EQ(svc_layers.spatial_idx(), sid);

        EXPECT_EQ(GetTemporalIndex(num_temporal_layers, frame_num),
                  metadata.temporal_idx);

        if (inter_layer_pred_mode == SVCInterLayerPredMode::kOnKeyPic) {
          VerifykSVCFrame(picture_param, metadata, ref_frames, frame_num,
                          config.active_spatial_layer_resolutions.size(),
                          config.begin_active_layer, config.end_active_layer);
        } else {
          VerifySmodeFrame(picture_param, metadata, ref_frames, frame_num,
                           config.active_spatial_layer_resolutions.size(),
                           config.begin_active_layer, config.end_active_layer);
        }

        if (j == call_get_times - 1) {
          for (size_t k = 0; k < kVp9NumRefFrames; ++k) {
            if (picture_param.refresh_frame_flags & (1 << k)) {
              ref_frames[k] = Vp9MetadataAndFrameNum{frame_num, metadata};
            }
          }

          svc_layers.PostEncode(picture_param.refresh_frame_flags);
        }
      }
    }
  }
}

// std::make_tuple(num_spatial_layers, num_temporal_layers,
// inter_layer_pred_mode)
INSTANTIATE_TEST_SUITE_P(
    ,
    VP9SVCLayersTest,
    ::testing::Values(std::make_tuple(1, 2, SVCInterLayerPredMode::kOff),
                      std::make_tuple(1, 3, SVCInterLayerPredMode::kOff),
                      std::make_tuple(1, 2, SVCInterLayerPredMode::kOn),
                      std::make_tuple(1, 3, SVCInterLayerPredMode::kOn),
                      std::make_tuple(1, 2, SVCInterLayerPredMode::kOnKeyPic),
                      std::make_tuple(1, 3, SVCInterLayerPredMode::kOnKeyPic),
                      std::make_tuple(2, 1, SVCInterLayerPredMode::kOnKeyPic),
                      std::make_tuple(2, 2, SVCInterLayerPredMode::kOnKeyPic),
                      std::make_tuple(2, 3, SVCInterLayerPredMode::kOnKeyPic),
                      std::make_tuple(3, 1, SVCInterLayerPredMode::kOnKeyPic),
                      std::make_tuple(3, 2, SVCInterLayerPredMode::kOnKeyPic),
                      std::make_tuple(3, 3, SVCInterLayerPredMode::kOnKeyPic),
                      std::make_tuple(2, 1, SVCInterLayerPredMode::kOff),
                      std::make_tuple(2, 2, SVCInterLayerPredMode::kOff),
                      std::make_tuple(2, 3, SVCInterLayerPredMode::kOff),
                      std::make_tuple(3, 1, SVCInterLayerPredMode::kOff),
                      std::make_tuple(3, 2, SVCInterLayerPredMode::kOff),
                      std::make_tuple(3, 3, SVCInterLayerPredMode::kOff)));
}  // namespace media
