// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vp9_svc_layers.h"

#include <algorithm>
#include <array>
#include <map>
#include <vector>

#include "base/containers/contains.h"
#include "media/filters/vp9_parser.h"
#include "media/gpu/vp9_picture.h"
#include "media/gpu/vp9_reference_frame_vector.h"
#include "media/video/video_encode_accelerator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

namespace {

constexpr gfx::Size kDefaultEncodeSize(1280, 720);
constexpr int kSpatialLayerResolutionDenom[] = {4, 2, 1};

std::vector<VP9SVCLayers::SpatialLayer> GetDefaultSVCLayers(
    size_t num_spatial_layers,
    size_t num_temporal_layers) {
  std::vector<VP9SVCLayers::SpatialLayer> spatial_layers;
  for (uint8_t i = 0; i < num_spatial_layers; ++i) {
    VP9SVCLayers::SpatialLayer spatial_layer;
    const int denom = kSpatialLayerResolutionDenom[i];
    spatial_layer.width = kDefaultEncodeSize.width() / denom;
    spatial_layer.height = kDefaultEncodeSize.height() / denom;
    spatial_layer.num_of_temporal_layers = num_temporal_layers;
    spatial_layers.push_back(spatial_layer);
  }
  return spatial_layers;
}

std::vector<gfx::Size> GetDefaultSVCResolutions(size_t num_spatial_layers) {
  std::vector<gfx::Size> spatial_layer_resolutions;
  for (size_t i = 0; i < num_spatial_layers; ++i) {
    const int denom = kSpatialLayerResolutionDenom[i];
    spatial_layer_resolutions.emplace_back(
        gfx::Size(kDefaultEncodeSize.width() / denom,
                  kDefaultEncodeSize.height() / denom));
  }
  return spatial_layer_resolutions;
}
}  // namespace

class VP9SVCLayersTest
    : public ::testing::TestWithParam<::testing::tuple<size_t, size_t>> {
 public:
  VP9SVCLayersTest() = default;
  ~VP9SVCLayersTest() = default;

 protected:
  void VerifyRefFrames(
      const Vp9FrameHeader& frame_hdr,
      const Vp9Metadata& metadata,
      const std::array<bool, kVp9NumRefsPerFrame>& ref_frames_used,
      const Vp9ReferenceFrameVector& ref_frames,
      const size_t num_spatial_layers,
      const bool key_pic);
  void VerifySVCStructure(bool keyframe,
                          size_t num_temporal_layers,
                          size_t num_spatial_layers,
                          const Vp9Metadata& metadata);

 private:
  std::vector<uint8_t> temporal_indices_[VP9SVCLayers::kMaxSpatialLayers];
  uint8_t spatial_index_;
};

void VP9SVCLayersTest::VerifySVCStructure(bool key_pic,
                                          size_t num_temporal_layers,
                                          size_t num_spatial_layers,
                                          const Vp9Metadata& metadata) {
  const uint8_t temporal_index = metadata.temporal_idx;
  const uint8_t spatial_index = metadata.spatial_idx;
  // Spatial index monotonically increases modulo |num_spatial_layers|.
  if (!key_pic || spatial_index != 0u)
    EXPECT_EQ((spatial_index_ + 1) % num_spatial_layers, spatial_index);

  spatial_index_ = spatial_index;
  auto& temporal_indices = temporal_indices_[spatial_index];
  if (key_pic) {
    temporal_indices.clear();
    temporal_indices.push_back(temporal_index);
    return;
  }
  EXPECT_FALSE(temporal_indices.empty());
  if (num_temporal_layers > 1)
    EXPECT_NE(temporal_indices.back(), temporal_index);
  else
    EXPECT_EQ(temporal_indices.back(), temporal_index);
  temporal_indices.push_back(temporal_index);
  if (spatial_index != num_spatial_layers - 1)
    return;
  // Check if the temporal layer structures in all spatial layers are identical.
  // Spatial index monotonically increases module |num_spatial_layers|.
  for (size_t i = 0; i < num_spatial_layers - 1; ++i)
    EXPECT_EQ(temporal_indices_[i], temporal_indices_[i + 1]);
  constexpr size_t kTemporalLayerCycle = 4u;
  constexpr size_t kSVCLayerCycle = kTemporalLayerCycle;
  if (temporal_indices.size() % kSVCLayerCycle == 0) {
    std::vector<size_t> count(num_temporal_layers, 0u);
    for (const uint8_t index : temporal_indices) {
      ASSERT_LE(index, num_temporal_layers);
      count[index]++;
    }
    // The number of frames in a higher temporal layer is not less than one in a
    // lower temporal layer.
    EXPECT_TRUE(std::is_sorted(count.begin(), count.end()));
  }
}

void VP9SVCLayersTest::VerifyRefFrames(
    const Vp9FrameHeader& frame_hdr,
    const Vp9Metadata& metadata,
    const std::array<bool, kVp9NumRefsPerFrame>& ref_frames_used,
    const Vp9ReferenceFrameVector& ref_frames,
    const size_t num_spatial_layers,
    const bool key_pic) {
  const uint8_t temporal_index = metadata.temporal_idx;
  const uint8_t spatial_index = metadata.spatial_idx;
  if (frame_hdr.IsKeyframe()) {
    EXPECT_EQ(frame_hdr.refresh_frame_flags, 0xff);
    EXPECT_FALSE(base::Contains(ref_frames_used, true));
    EXPECT_EQ(temporal_index, 0u);
    EXPECT_EQ(spatial_index, 0u);
    EXPECT_EQ(metadata.referenced_by_upper_spatial_layers,
              num_spatial_layers > 1);
    EXPECT_FALSE(metadata.reference_lower_spatial_layers);
    EXPECT_TRUE(metadata.p_diffs.empty());
    EXPECT_EQ(metadata.spatial_layer_resolutions,
              GetDefaultSVCResolutions(num_spatial_layers));
    return;
  }

  // Six slots at most in the reference pool are used in spatial/temporal layer
  // encoding. Additionally, non-keyframe must reference some frames.
  // |ref_frames_used| must be {true, false, false} because here is,
  // 1. if the frame is in key picture, it references one lower spatial layer,
  // 2. otherwise the frame doesn't reference other spatial layers and thus
  // references only one frame in the same spatial layer based on the current
  // reference pattern.
  constexpr std::array<bool, kVp9NumRefsPerFrame> kExpectedRefFramesUsed = {
      true, false, false};
  EXPECT_EQ(frame_hdr.refresh_frame_flags & ~(0b111111u), 0u);
  EXPECT_EQ(ref_frames_used, kExpectedRefFramesUsed);
  EXPECT_EQ(metadata.inter_pic_predicted, !metadata.p_diffs.empty());
  EXPECT_EQ(metadata.inter_pic_predicted, !key_pic);
  if (key_pic) {
    EXPECT_TRUE(metadata.reference_lower_spatial_layers);
    EXPECT_EQ(metadata.referenced_by_upper_spatial_layers,
              spatial_index + 1 != num_spatial_layers);
  } else {
    EXPECT_FALSE(metadata.referenced_by_upper_spatial_layers);
    EXPECT_FALSE(metadata.reference_lower_spatial_layers);
  }

  // Check that the current frame doesn't reference upper layer frames.
  const uint8_t index = frame_hdr.ref_frame_idx[0];
  scoped_refptr<VP9Picture> ref_frame = ref_frames.GetFrame(index);
  ASSERT_TRUE(!!ref_frame);
  const auto& ref_metadata = ref_frame->metadata_for_encoding;
  ASSERT_TRUE(ref_metadata.has_value());
  const size_t ref_temporal_index = ref_metadata->temporal_idx;
  EXPECT_LE(ref_temporal_index, temporal_index);
  const uint8_t ref_spatial_index = ref_metadata->spatial_idx;
  EXPECT_LE(ref_spatial_index, spatial_index);
  // In key picture, upper spatial layers must refer the lower spatial layer.
  // Or referenced frames must be in the same spatial layer.
  if (key_pic)
    EXPECT_EQ(ref_spatial_index, spatial_index - 1);
  else
    EXPECT_EQ(ref_spatial_index, spatial_index);
}

// This test verifies the bitrate check in MaybeUpdateActiveLayer().
TEST_F(VP9SVCLayersTest, MaybeUpdateActiveLayer) {
  constexpr size_t kNumSpatialLayers = 3;
  constexpr size_t kNumTemporalLayers = 3;
  const std::vector<VP9SVCLayers::SpatialLayer> spatial_layers =
      GetDefaultSVCLayers(kNumSpatialLayers, kNumTemporalLayers);
  VP9SVCLayers svc_layers(spatial_layers);

  // Set Default bitrate allocation.
  uint32_t layer_rate = 1u;
  VideoBitrateAllocation allocation;
  for (size_t sid = 0; sid < VideoBitrateAllocation::kMaxSpatialLayers; ++sid) {
    for (size_t tid = 0; tid < VideoBitrateAllocation::kMaxTemporalLayers;
         ++tid) {
      allocation.SetBitrate(sid, tid, layer_rate++);
    }
  }
  DCHECK_LT(kNumSpatialLayers, VideoBitrateAllocation::kMaxSpatialLayers);
  DCHECK_LT(kNumTemporalLayers, VideoBitrateAllocation::kMaxTemporalLayers);
  EXPECT_FALSE(svc_layers.MaybeUpdateActiveLayer(&allocation));

  // Set unsupported temporal layer bitrate to 0.
  for (size_t sid = 0; sid < VideoBitrateAllocation::kMaxSpatialLayers; ++sid) {
    for (size_t tid = kNumTemporalLayers;
         tid < VideoBitrateAllocation::kMaxTemporalLayers; ++tid) {
      allocation.SetBitrate(sid, tid, 0u);
    }
  }
  EXPECT_FALSE(svc_layers.MaybeUpdateActiveLayer(&allocation));

  // Set unsupported spatial layer bitrate to 0.
  for (size_t sid = kNumSpatialLayers;
       sid < VideoBitrateAllocation::kMaxSpatialLayers; ++sid) {
    for (size_t tid = 0; tid < VideoBitrateAllocation::kMaxTemporalLayers;
         ++tid) {
      allocation.SetBitrate(sid, tid, 0u);
    }
  }
  EXPECT_TRUE(svc_layers.MaybeUpdateActiveLayer(&allocation));

  // Set lower temporal layer bitrate to zero, e.g. {0, 2, 3}.
  allocation.SetBitrate(/*spatial_index=*/0, /*temporal_index=*/0, 0u);
  EXPECT_FALSE(svc_layers.MaybeUpdateActiveLayer(&allocation));

  allocation.SetBitrate(/*spatial_index=*/0, /*temporal_index=*/0, 1u);
  // Set upper temporal layer bitrate to 0, e.g. {1, 2, 0}.
  allocation.SetBitrate(/*spatial_index=*/0, /*temporal_index=*/2, 0u);
  EXPECT_FALSE(svc_layers.MaybeUpdateActiveLayer(&allocation));

  allocation.SetBitrate(/*spatial_index=*/0, /*temporal_index=*/2, 3u);
  // Deactivate SL0 and SL1 and verify the new bitrate allocation.
  constexpr int kNumDeactivatedLowerSpatialLayer = 2;
  VideoBitrateAllocation new_allocation = allocation;
  for (size_t sid = 0; sid < kNumDeactivatedLowerSpatialLayer; ++sid) {
    for (size_t tid = 0; tid < kNumTemporalLayers; ++tid)
      new_allocation.SetBitrate(sid, tid, 0u);
  }
  EXPECT_TRUE(svc_layers.MaybeUpdateActiveLayer(&new_allocation));
  for (size_t sid = 0; sid < kNumSpatialLayers; ++sid) {
    for (size_t tid = 0; tid < kNumTemporalLayers; ++tid) {
      if (sid + kNumDeactivatedLowerSpatialLayer <
          VideoBitrateAllocation::kMaxSpatialLayers)
        EXPECT_EQ(new_allocation.GetBitrateBps(sid, tid),
                  allocation.GetBitrateBps(
                      sid + kNumDeactivatedLowerSpatialLayer, tid));
      else
        EXPECT_EQ(new_allocation.GetBitrateBps(sid, tid), 0u);
    }
  }

  // Deactivate SL2 and verify the new bitrate allocation.
  new_allocation = allocation;
  constexpr int kNumActiveSpatialLayer = 2;
  for (size_t tid = 0; tid < kNumTemporalLayers; ++tid)
    new_allocation.SetBitrate(/*spatial_index=*/2, tid, 0u);
  EXPECT_TRUE(svc_layers.MaybeUpdateActiveLayer(&new_allocation));
  for (size_t sid = 0; sid < kNumSpatialLayers; ++sid) {
    for (size_t tid = 0; tid < kNumTemporalLayers; ++tid) {
      if (sid < kNumActiveSpatialLayer)
        EXPECT_EQ(new_allocation.GetBitrateBps(sid, tid),
                  allocation.GetBitrateBps(sid, tid));
      else
        EXPECT_EQ(new_allocation.GetBitrateBps(sid, tid), 0u);
    }
  }
}

TEST_P(VP9SVCLayersTest, ) {
  const size_t num_spatial_layers = ::testing::get<0>(GetParam());
  const size_t num_temporal_layers = ::testing::get<1>(GetParam());

  const std::vector<VP9SVCLayers::SpatialLayer> spatial_layers =
      GetDefaultSVCLayers(num_spatial_layers, num_temporal_layers);
  VP9SVCLayers svc_layers(spatial_layers);

  constexpr size_t kNumFramesToEncode = 32;
  Vp9ReferenceFrameVector ref_frames;
  constexpr size_t kKeyFrameInterval = 10;
  for (size_t frame_num = 0; frame_num < kNumFramesToEncode; ++frame_num) {
    // True iff the picture in the bottom spatial layer is key frame.
    bool key_pic = false;
    for (size_t sid = 0; sid < num_spatial_layers; ++sid) {
      scoped_refptr<VP9Picture> picture(new VP9Picture);
      picture->frame_hdr = std::make_unique<Vp9FrameHeader>();
      const bool keyframe = svc_layers.UpdateEncodeJob(
          /*is_key_frame_requested=*/false, kKeyFrameInterval);
      picture->frame_hdr->frame_type =
          keyframe ? Vp9FrameHeader::KEYFRAME : Vp9FrameHeader::INTERFRAME;
      if (sid == 0)
        key_pic = keyframe;
      std::array<bool, kVp9NumRefsPerFrame> ref_frames_used;
      svc_layers.FillUsedRefFramesAndMetadata(picture.get(), &ref_frames_used);
      ASSERT_TRUE(picture->metadata_for_encoding.has_value());
      VerifyRefFrames(*picture->frame_hdr, *picture->metadata_for_encoding,
                      ref_frames_used, ref_frames, num_spatial_layers, key_pic);
      VerifySVCStructure(key_pic, num_temporal_layers, num_spatial_layers,
                         *picture->metadata_for_encoding);
      ref_frames.Refresh(picture);
    }
  }
}

// std::make_tuple(num_spatial_layers, num_temporal_layers)
INSTANTIATE_TEST_SUITE_P(,
                         VP9SVCLayersTest,
                         ::testing::Values(std::make_tuple(1, 2),
                                           std::make_tuple(1, 3),
                                           std::make_tuple(2, 1),
                                           std::make_tuple(2, 2),
                                           std::make_tuple(2, 3),
                                           std::make_tuple(3, 1),
                                           std::make_tuple(3, 2),
                                           std::make_tuple(3, 3)));

}  // namespace media
