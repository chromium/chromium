// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/video_bitrate_suggester.h"

#include <memory>
#include <numeric>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/common/openscreen_conversion_helpers.h"
#include "media/cast/test/openscreen_test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media::cast {
namespace {

constexpr uint32_t kFirstSsrc = 35535;
constexpr int kRtpTimebase = 9000;

static const FrameSenderConfig kVideoConfig{
    kFirstSsrc + 2,
    kFirstSsrc + 3,
    base::Milliseconds(100),
    kDefaultTargetPlayoutDelay,
    /* use_hardware_encoder= */ false,
    kRtpTimebase,
    /* channels = */ 1,
    kDefaultMaxVideoBitrate,
    kDefaultMinVideoBitrate,
    std::midpoint<int>(kDefaultMinVideoBitrate, kDefaultMaxVideoBitrate),
    kDefaultMaxFrameRate,
    VideoCodecParams(VideoCodec::kVP8),
    std::nullopt};

}  // namespace

class VideoBitrateSuggesterTest : public ::testing::Test {
 public:
  int get_suggested_bitrate() { return suggested_bitrate_; }

 protected:
  VideoBitrateSuggesterTest() {
    suggester_ = std::make_unique<VideoBitrateSuggester>(
        kVideoConfig,
        base::BindRepeating(&VideoBitrateSuggesterTest::get_suggested_bitrate,
                            // Safe because we destroy the audio sender before
                            // destroying `this`.
                            base::Unretained(this)));
  }

  void RecordShouldDropNextFrame(bool should_drop) {
    suggester_->RecordShouldDropNextFrame(should_drop);
  }

  void set_suggested_bitrate(int bitrate) { suggested_bitrate_ = bitrate; }

  VideoBitrateSuggester& suggester() { return *suggester_; }

  void UseExponentialAlgorithm() {
    feature_list_.InitAndEnableFeature(
        media::kCastStreamingExponentialVideoBitrateAlgorithm);
  }

  void UseLinearAlgorithm() {
    feature_list_.InitAndDisableFeature(
        media::kCastStreamingExponentialVideoBitrateAlgorithm);
  }

 private:
  std::unique_ptr<VideoBitrateSuggester> suggester_;
  base::test::ScopedFeatureList feature_list_;
  int suggested_bitrate_ = 0;
};

TEST_F(VideoBitrateSuggesterTest, StaysWithinBounds) {
  set_suggested_bitrate(10000000);
  EXPECT_EQ(kDefaultMaxVideoBitrate, suggester().GetSuggestedBitrate());

  set_suggested_bitrate(1);
  EXPECT_EQ(kDefaultMinVideoBitrate, suggester().GetSuggestedBitrate());
}

TEST_F(VideoBitrateSuggesterTest,
       SuggestsBitratesCorrectlyWithExponentialAlgorithm) {
  UseExponentialAlgorithm();

  // We should start with the maximum video bitrate.
  set_suggested_bitrate(5000001);
  EXPECT_EQ(kDefaultMaxVideoBitrate, suggester().GetSuggestedBitrate());

  // After a period with multiple frame drops, this should go down.
  RecordShouldDropNextFrame(true);
  RecordShouldDropNextFrame(true);
  for (int i = 0; i < 29; ++i) {
    RecordShouldDropNextFrame(false);
  }

  // It should continue to go down to the minimum as long as frames are being
  // dropped.
  int last_suggestion = suggester().GetSuggestedBitrate();
  EXPECT_EQ(3500000, last_suggestion);
  while (last_suggestion > kDefaultMinVideoBitrate) {
    RecordShouldDropNextFrame(true);
    for (int j = 0; j < 29; ++j) {
      RecordShouldDropNextFrame(false);
    }

    // It should drop every time.
    const int suggestion = suggester().GetSuggestedBitrate();
    EXPECT_LT(suggestion, last_suggestion);
    last_suggestion = suggestion;
  }

  // And then stabilize at the bottom.
  EXPECT_EQ(kDefaultMinVideoBitrate, suggester().GetSuggestedBitrate());

  // It should increase once we stop dropping frames.
  last_suggestion = kDefaultMinVideoBitrate;
  while (last_suggestion < kDefaultMaxVideoBitrate) {
    for (int j = 0; j < 30; ++j) {
      RecordShouldDropNextFrame(false);
    }
    const int suggestion = suggester().GetSuggestedBitrate();
    EXPECT_GT(suggestion, last_suggestion);
    last_suggestion = suggestion;
  }

  // And stop at the maximum.
  EXPECT_EQ(kDefaultMaxVideoBitrate, suggester().GetSuggestedBitrate());

  // Finally, it should cap at the bitrate suggested by Open Screen.
  set_suggested_bitrate(4998374);
  EXPECT_EQ(4998374, suggester().GetSuggestedBitrate());
}

TEST_F(VideoBitrateSuggesterTest,
       SuggestsBitratesCorrectlyWithLinearAlgorithm) {
  UseLinearAlgorithm();

  // We should start with the maximum video bitrate.
  set_suggested_bitrate(5000001);
  EXPECT_EQ(kDefaultMaxVideoBitrate, suggester().GetSuggestedBitrate());

  // After a period with multiple frame drops, this should go down.
  RecordShouldDropNextFrame(true);
  RecordShouldDropNextFrame(true);
  for (int i = 0; i < 99; ++i) {
    RecordShouldDropNextFrame(false);
  }

  // It should now go down.
  int last_suggestion = suggester().GetSuggestedBitrate();
  EXPECT_EQ(4412500, last_suggestion);

  // It should continue to go down to the minimum as long as frames are being
  // dropped.
  while (last_suggestion > kDefaultMinVideoBitrate) {
    RecordShouldDropNextFrame(true);
    for (int j = 0; j < 99; ++j) {
      RecordShouldDropNextFrame(false);
    }

    // It should drop every time.
    const int suggestion = suggester().GetSuggestedBitrate();
    EXPECT_LT(suggestion, last_suggestion);
    last_suggestion = suggestion;
  }

  // It should increase once we stop dropping frames.
  last_suggestion = suggester().GetSuggestedBitrate();
  EXPECT_EQ(kDefaultMinVideoBitrate, last_suggestion);
  while (last_suggestion < kDefaultMaxVideoBitrate) {
    for (int j = 0; j < 100; ++j) {
      RecordShouldDropNextFrame(false);
    }
    const int suggestion = suggester().GetSuggestedBitrate();
    EXPECT_GT(suggestion, last_suggestion);
    last_suggestion = suggestion;
  }

  // And stop at the maximum.
  EXPECT_EQ(kDefaultMaxVideoBitrate, suggester().GetSuggestedBitrate());

  // Finally, it should cap at the bitrate suggested by Open Screen.
  set_suggested_bitrate(4998374);
  EXPECT_EQ(4998374, suggester().GetSuggestedBitrate());
}
}  // namespace media::cast
