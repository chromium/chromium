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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media::cast {
namespace {

constexpr uint32_t kFirstSsrc = 35535;
constexpr int kRtpTimebase = 9000;
constexpr char kAesSecretKey[] = "65386FD9BCC30BC7FB6A4DD1D3B0FA5E";
constexpr char kAesIvMask[] = "64A6AAC2821880145271BB15B0188821";

static const FrameSenderConfig kVideoConfig{
    kFirstSsrc + 2,
    kFirstSsrc + 3,
    base::Milliseconds(100),
    kDefaultTargetPlayoutDelay,
    RtpPayloadType::VIDEO_VP8,
    /* use_hardware_encoder= */ false,
    kRtpTimebase,
    /* channels = */ 1,
    kDefaultMaxVideoBitrate,
    kDefaultMinVideoBitrate,
    std::midpoint<int>(kDefaultMinVideoBitrate, kDefaultMaxVideoBitrate),
    kDefaultMaxFrameRate,
    kAesSecretKey,
    kAesIvMask,
    VideoCodecParams(VideoCodec::kVP8),
    std::nullopt};
static const openscreen::cast::SessionConfig kOpenscreenVideoConfig =
    ToOpenscreenSessionConfig(kVideoConfig, /* is_pli_enabled= */ true);

}  // namespace

class VideoBitrateSuggesterTest : public ::testing::Test {
 public:
  int get_suggested_bitrate() { return suggested_bitrate_; }

 protected:
  VideoBitrateSuggesterTest() {
    video_bitrate_suggester_ = std::make_unique<VideoBitrateSuggester>(
        kVideoConfig,
        base::BindRepeating(&VideoBitrateSuggesterTest::get_suggested_bitrate,
                            // Safe because we destroy the audio sender before
                            // destroying `this`.
                            base::Unretained(this)));
  }

  void RecordShouldDropNextFrame(bool should_drop) {
    video_bitrate_suggester_->RecordShouldDropNextFrame(should_drop);
  }

  void set_suggested_bitrate(int bitrate) { suggested_bitrate_ = bitrate; }

  VideoBitrateSuggester& video_bitrate_suggester() {
    return *video_bitrate_suggester_;
  }

  void UseExponentialAlgorithm() {
    feature_list_.InitAndEnableFeature(
        media::kCastStreamingExponentialVideoBitrateAlgorithm);
  }

  void UseLinearAlgorithm() {
    feature_list_.InitAndDisableFeature(
        media::kCastStreamingExponentialVideoBitrateAlgorithm);
  }

 private:
  std::unique_ptr<VideoBitrateSuggester> video_bitrate_suggester_;
  base::test::ScopedFeatureList feature_list_;
  int suggested_bitrate_ = 0;
};

TEST_F(VideoBitrateSuggesterTest,
       SuggestsBitratesCorrectlyWithExponentialAlgorithm) {
  UseExponentialAlgorithm();

  // We should start with the maximum video bitrate.
  set_suggested_bitrate(5000001);
  EXPECT_EQ(5000000, video_bitrate_suggester().GetSuggestedBitrate());

  // After a period with multiple frame drops, this should go down.
  RecordShouldDropNextFrame(true);
  RecordShouldDropNextFrame(true);
  for (int i = 0; i < 29; ++i) {
    RecordShouldDropNextFrame(false);
  }

  // It should now go down.
  EXPECT_EQ(4000000, video_bitrate_suggester().GetSuggestedBitrate());

  // It should continue to go down to the minimum as long as frames are being
  // dropped.
  int last_suggestion = 4685120;
  for (int i = 0; i < 12; ++i) {
    RecordShouldDropNextFrame(true);
    for (int j = 0; j < 29; ++j) {
      RecordShouldDropNextFrame(false);
    }

    // It should drop every time.
    const int suggestion = video_bitrate_suggester().GetSuggestedBitrate();
    EXPECT_LT(suggestion, last_suggestion);
    last_suggestion = suggestion;
  }

  // And then stabilize at the bottom.
  EXPECT_EQ(300000, video_bitrate_suggester().GetSuggestedBitrate());

  // It should increase once we stop dropping frames.
  last_suggestion = 300000;
  for (int i = 0; i < 30; ++i) {
    for (int j = 0; j < 30; ++j) {
      RecordShouldDropNextFrame(false);
    }
    const int suggestion = video_bitrate_suggester().GetSuggestedBitrate();
    EXPECT_GT(suggestion, last_suggestion);
    last_suggestion = suggestion;
  }

  // And stop at the maximum.
  EXPECT_EQ(5000000, video_bitrate_suggester().GetSuggestedBitrate());

  // Finally, it should cap at the bitrate suggested by Open Screen.
  set_suggested_bitrate(4998374);
  EXPECT_EQ(4998374, video_bitrate_suggester().GetSuggestedBitrate());
}

TEST_F(VideoBitrateSuggesterTest,
       SuggestsBitratesCorrectlyWithLinearAlgorithm) {
  UseLinearAlgorithm();

  // We should start with the maximum video bitrate.
  set_suggested_bitrate(5000001);
  EXPECT_EQ(5000000, video_bitrate_suggester().GetSuggestedBitrate());

  // After a period with multiple frame drops, this should go down.
  RecordShouldDropNextFrame(true);
  RecordShouldDropNextFrame(true);
  for (int i = 0; i < 99; ++i) {
    RecordShouldDropNextFrame(false);
  }

  // It should now go down.
  EXPECT_EQ(4412500, video_bitrate_suggester().GetSuggestedBitrate());

  // It should continue to go down to the minimum as long as frames are being
  // dropped.
  int last_suggestion = 4412500;
  for (int i = 0; i < 7; ++i) {
    RecordShouldDropNextFrame(true);
    for (int j = 0; j < 99; ++j) {
      RecordShouldDropNextFrame(false);
    }

    // It should drop every time.
    const int suggestion = video_bitrate_suggester().GetSuggestedBitrate();
    EXPECT_LT(suggestion, last_suggestion);
    last_suggestion = suggestion;
  }

  // And then stabilize at the bottom.
  EXPECT_EQ(300000, video_bitrate_suggester().GetSuggestedBitrate());

  // It should increase once we stop dropping frames.
  last_suggestion = 300000;
  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 100; ++j) {
      RecordShouldDropNextFrame(false);
    }
    const int suggestion = video_bitrate_suggester().GetSuggestedBitrate();
    EXPECT_GT(suggestion, last_suggestion);
    last_suggestion = suggestion;
  }

  // And stop at the maximum.
  EXPECT_EQ(5000000, video_bitrate_suggester().GetSuggestedBitrate());

  // Finally, it should cap at the bitrate suggested by Open Screen.
  set_suggested_bitrate(4998374);
  EXPECT_EQ(4998374, video_bitrate_suggester().GetSuggestedBitrate());
}
}  // namespace media::cast
