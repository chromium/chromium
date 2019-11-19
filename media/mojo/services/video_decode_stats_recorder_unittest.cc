// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "media/base/video_codecs.h"
#include "media/learning/common/value.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "media/mojo/services/test_helpers.h"
#include "media/mojo/services/video_decode_stats_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

using testing::_;

namespace media {

namespace {

// Aliases for readability.
const ukm::SourceId kSourceIdA = 1u;
const ukm::SourceId kSourceIdB = 2u;
const bool kIsTopFrameA = true;
const bool kIsTopFrameB = false;
const uint64_t kPlayerIdA = 1234u;
const uint64_t kPlayerIdB = 5678u;
const VideoCodecProfile kProfileA = H264PROFILE_MIN;
const VideoCodecProfile kProfileB = VP9PROFILE_MIN;
const int kFpsA = 30;
const int kFpsB = 60;

MATCHER_P(MojoEq, value, "") {
  return arg.Equals(value);
}

}  // namespace

class VideoDecodeStatsRecorderTest : public ::testing::Test {
 public:
  VideoDecodeStatsRecorderTest()
      : kOriginA_("http://example.com"),
        kOriginB_("https://google.com"),
        kSizeA_(1280, 720),
        kSizeB_(640, 360) {}

  ~VideoDecodeStatsRecorderTest() override = default;

  void SetUp() override {}

  void TearDown() override {}

  void MakeRecorder(ukm::SourceId source_id,
                    learning::FeatureValue origin,
                    bool is_top_frame,
                    uint64_t player_id) {
    recorder_.reset(new VideoDecodeStatsRecorder(
        base::BindRepeating(&VideoDecodeStatsRecorderTest::PrintSavePerfRecord,
                            base::Unretained(this)),
        source_id, origin, is_top_frame, player_id));
  }

  void PrintSavePerfRecord(ukm::SourceId source_id,
                           learning::FeatureValue origin,
                           bool is_top_frame,
                           mojom::PredictionFeatures features,
                           mojom::PredictionTargets targets,
                           uint64_t player_id,
                           base::OnceClosure save_done_cb) {
    SavePerfRecord(source_id, origin, is_top_frame, features, targets,
                   player_id, std::move(save_done_cb));
  }

  MOCK_METHOD7(SavePerfRecord,
               void(ukm::SourceId source_id,
                    learning::FeatureValue origin,
                    bool is_top_frame,
                    mojom::PredictionFeatures features,
                    mojom::PredictionTargets targets,
                    uint64_t player_id,
                    base::OnceClosure save_done_cb));

 protected:
  std::unique_ptr<VideoDecodeStatsRecorder> recorder_;

  // Class members to avoid static initializer.
  const learning::Value kOriginA_;
  const learning::Value kOriginB_;
  const gfx::Size kSizeA_;
  const gfx::Size kSizeB_;
};

TEST_F(VideoDecodeStatsRecorderTest, SaveOnStartNewRecord) {
  MakeRecorder(kSourceIdA, kOriginA_, kIsTopFrameA, kPlayerIdA);
  recorder_->StartNewRecord(MakeFeaturesPtr(kProfileA, kSizeA_, kFpsA));
  recorder_->UpdateRecord(MakeTargetsPtr(3, 2, 1));

  // Expect save with all the 'A' state upon starting a record with 'B' state.
  EXPECT_CALL(*this,
              SavePerfRecord(kSourceIdA, kOriginA_, kIsTopFrameA,
                             MojoEq(MakeFeatures(kProfileA, kSizeA_, kFpsA)),
                             MojoEq(MakeTargets(3, 2, 1)), kPlayerIdA, _));
  recorder_->StartNewRecord(MakeFeaturesPtr(kProfileB, kSizeB_, kFpsB));

  // Update 'B's counts and test the opposite direction by starting a new record
  // again using 'A' state. Note, this mixes A's construction state w/ B's
  // "features".
  recorder_->UpdateRecord(MakeTargetsPtr(6, 5, 4));
  EXPECT_CALL(*this,
              SavePerfRecord(kSourceIdA, kOriginA_, kIsTopFrameA,
                             MojoEq(MakeFeatures(kProfileB, kSizeB_, kFpsB)),
                             MojoEq(MakeTargets(6, 5, 4)), kPlayerIdA, _));
  recorder_->StartNewRecord(MakeFeaturesPtr(kProfileA, kSizeA_, kFpsA));
}

TEST_F(VideoDecodeStatsRecorderTest, SaveOnDestruction) {
  MakeRecorder(kSourceIdA, kOriginA_, kIsTopFrameA, kPlayerIdA);
  recorder_->StartNewRecord(MakeFeaturesPtr(kProfileA, kSizeA_, kFpsA));
  recorder_->UpdateRecord(MakeTargetsPtr(3, 2, 1));

  // Expect save with all the 'A' state upon destruction.
  EXPECT_CALL(*this,
              SavePerfRecord(kSourceIdA, kOriginA_, kIsTopFrameA,
                             MojoEq(MakeFeatures(kProfileA, kSizeA_, kFpsA)),
                             MojoEq(MakeTargets(3, 2, 1)), kPlayerIdA, _));
  recorder_.reset();

  // Repeat with 'B' state just to be sure
  MakeRecorder(kSourceIdB, kOriginB_, kIsTopFrameB, kPlayerIdB);
  recorder_->StartNewRecord(MakeFeaturesPtr(kProfileB, kSizeB_, kFpsB));
  recorder_->UpdateRecord(MakeTargetsPtr(3, 2, 1));

  // Expect save with all the 'B' state upon destruction.
  EXPECT_CALL(*this,
              SavePerfRecord(kSourceIdB, kOriginB_, kIsTopFrameB,
                             MojoEq(MakeFeatures(kProfileB, kSizeB_, kFpsB)),
                             MojoEq(MakeTargets(3, 2, 1)), kPlayerIdB, _));
  recorder_.reset();
}

}  // namespace media
