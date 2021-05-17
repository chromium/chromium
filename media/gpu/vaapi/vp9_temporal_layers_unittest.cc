// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vp9_temporal_layers.h"

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
class VP9TemporalLayersTest : public ::testing::TestWithParam<size_t> {
 public:
  VP9TemporalLayersTest() = default;
  ~VP9TemporalLayersTest() = default;

 protected:
  void VerifyRefFrames(
      const Vp9FrameHeader& frame_hdr,
      const Vp9Metadata& metadata,
      const std::array<bool, kVp9NumRefsPerFrame>& ref_frames_used,
      const Vp9ReferenceFrameVector& ref_frames);
  void VerifyTemporalLayersStructure(bool keyframe,
                                     size_t num_layers,
                                     const Vp9Metadata& metadata);

 private:
  std::vector<uint8_t> temporal_indices_;
};

void VP9TemporalLayersTest::VerifyTemporalLayersStructure(
    bool keyframe,
    size_t num_layers,
    const Vp9Metadata& metadata) {
  const uint8_t temporal_index = metadata.temporal_idx;
  if (keyframe) {
    temporal_indices_.clear();
    temporal_indices_.push_back(temporal_index);
    return;
  }

  ASSERT_FALSE(temporal_indices_.empty());
  EXPECT_NE(temporal_indices_.back(), temporal_index);

  // Check that the number of temporal layer ids are expected.
  temporal_indices_.push_back(temporal_index);
  constexpr size_t kTemporalLayerCycle = 4u;
  if (temporal_indices_.size() % kTemporalLayerCycle == 0) {
    std::vector<size_t> count(num_layers, 0u);
    for (const uint8_t index : temporal_indices_) {
      ASSERT_LE(index, num_layers);
      count[index]++;
    }

    // The number of frames in a higher layer is not less than one in a lower
    // layer.
    EXPECT_TRUE(std::is_sorted(count.begin(), count.end()));
  }
}

void VP9TemporalLayersTest::VerifyRefFrames(
    const Vp9FrameHeader& frame_hdr,
    const Vp9Metadata& metadata,
    const std::array<bool, kVp9NumRefsPerFrame>& ref_frames_used,
    const Vp9ReferenceFrameVector& ref_frames) {
  const uint8_t current_temporal_index = metadata.temporal_idx;
  if (frame_hdr.IsKeyframe()) {
    EXPECT_EQ(frame_hdr.refresh_frame_flags, 0xff);
    EXPECT_FALSE(base::Contains(ref_frames_used, true));
    EXPECT_EQ(current_temporal_index, 0u);
    return;
  }

  // Two slots at most in the reference pool are used in temporal layer
  // encoding. Additionally, non-keyframe must reference some frames.
  EXPECT_EQ(frame_hdr.refresh_frame_flags & ~(0b11u), 0u);
  EXPECT_FALSE(ref_frames_used[VP9TemporalLayers::kMaxNumUsedReferenceFrames]);
  EXPECT_TRUE(base::Contains(ref_frames_used, true));
  EXPECT_TRUE(metadata.has_reference);

  // Check that the current frame doesn't reference upper layer frames.
  for (size_t i = 0; i < kVp9NumRefsPerFrame; i++) {
    if (!ref_frames_used[i])
      continue;
    const uint8_t index = frame_hdr.ref_frame_idx[i];
    scoped_refptr<VP9Picture> ref_frame = ref_frames.GetFrame(index);
    ASSERT_TRUE(!!ref_frame);
    const auto& ref_metadata = ref_frame->metadata_for_encoding;
    ASSERT_TRUE(ref_metadata.has_value());
    const size_t ref_temporal_index = ref_metadata->temporal_idx;
    EXPECT_LE(ref_temporal_index, current_temporal_index);
  }
  return;
}

TEST_P(VP9TemporalLayersTest, ) {
  const size_t num_temporal_layers = GetParam();
  VP9TemporalLayers temporal_layers(num_temporal_layers);
  EXPECT_EQ(temporal_layers.num_layers(), num_temporal_layers);

  constexpr size_t kNumFramesToEncode = 32;
  Vp9ReferenceFrameVector ref_frames;
  constexpr size_t kKeyFrameInterval = 16;
  for (size_t i = 0; i < kNumFramesToEncode; i++) {
    const bool keyframe = i % kKeyFrameInterval == 0;
    scoped_refptr<VP9Picture> picture(new VP9Picture);
    picture->frame_hdr = std::make_unique<Vp9FrameHeader>();
    picture->frame_hdr->frame_type =
        keyframe ? Vp9FrameHeader::KEYFRAME : Vp9FrameHeader::INTERFRAME;
    std::array<bool, kVp9NumRefsPerFrame> ref_frames_used;
    temporal_layers.FillUsedRefFramesAndMetadata(picture.get(),
                                                 &ref_frames_used);
    ASSERT_TRUE(picture->metadata_for_encoding.has_value());
    VerifyRefFrames(*picture->frame_hdr, *picture->metadata_for_encoding,
                    ref_frames_used, ref_frames);
    VerifyTemporalLayersStructure(keyframe, num_temporal_layers,
                                  *picture->metadata_for_encoding);
    ref_frames.Refresh(picture);
  }
}

constexpr size_t kNumTemporalLayers[] = {
    VP9TemporalLayers::kMinSupportedTemporalLayers,
    VP9TemporalLayers::kMaxSupportedTemporalLayers,
};

INSTANTIATE_TEST_SUITE_P(,
                         VP9TemporalLayersTest,
                         ::testing::ValuesIn(kNumTemporalLayers));

}  // namespace media
