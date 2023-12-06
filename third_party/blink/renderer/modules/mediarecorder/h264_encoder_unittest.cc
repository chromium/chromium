// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/h264_encoder.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/base/mock_filters.h"
#include "media/base/video_codecs.h"
#include "media/base/video_encoder.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/modules/mediarecorder/video_track_recorder.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_media.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {
namespace {

struct TestParam {
  absl::optional<media::VideoCodecProfile> profile;
  absl::optional<uint8_t> level;
  uint32_t bitrate;
};

const int kFrameWidth = 64;
const int kFrameHeight = 64;

const TestParam kH264EncoderParameterTestParam[] = {
    {media::VideoCodecProfile::H264PROFILE_BASELINE, 50,
     kFrameWidth* kFrameHeight * 2},
    {media::VideoCodecProfile::H264PROFILE_MAIN, 51,
     kFrameWidth* kFrameHeight * 4},
    {media::VideoCodecProfile::H264PROFILE_HIGH, 52,
     kFrameWidth* kFrameHeight * 8},
    // Test optional input.
    {absl::nullopt, absl::nullopt, kFrameWidth* kFrameHeight * 8},
};

}  // namespace

class H264EncoderFixture : public ::testing::Test {
 public:
  H264EncoderFixture(absl::optional<media::VideoCodecProfile> profile,
                     absl::optional<uint8_t> level,
                     uint32_t bitrate)
      : profile_(profile),
        level_(level),
        bitrate_(bitrate),
        encoder_(
            scheduler::GetSingleThreadTaskRunnerForTesting(),
            ConvertToBaseRepeatingCallback(
                CrossThreadBindRepeating(&H264EncoderFixture::OnEncodedVideo,
                                         CrossThreadUnretained(this))),
            VideoTrackRecorder::CodecProfile(VideoTrackRecorder::CodecId::kH264,
                                             profile_,
                                             level_),
            bitrate_) {
    auto metrics_provider =
        std::make_unique<media::MockVideoEncoderMetricsProvider>();
    mock_metrics_provider_ = metrics_provider.get();
    encoder_.metrics_provider_ = std::move(metrics_provider);
  }

  H264EncoderFixture(const H264EncoderFixture&) = delete;
  H264EncoderFixture& operator=(const H264EncoderFixture&) = delete;

 protected:
  void EncodeFrame() {
    encoder_.StartFrameEncode(
        CrossThreadBindRepeating(base::TimeTicks::Now),
        media::VideoFrame::CreateBlackFrame({kFrameWidth, kFrameHeight}),
        base::TimeTicks::Now());
  }

  std::pair<media::VideoCodecProfile, uint8_t> GetProfileLevelForTesting() {
    static const HashMap<EProfileIdc, media::VideoCodecProfile>
        kEProfileIdcToProfile({
            {PRO_BASELINE, media::H264PROFILE_BASELINE},
            {PRO_MAIN, media::H264PROFILE_MAIN},
            {PRO_EXTENDED, media::H264PROFILE_EXTENDED},
            {PRO_HIGH, media::H264PROFILE_HIGH},
        });

    static const HashMap<ELevelIdc, uint8_t> kELevelIdcToLevel({
        {LEVEL_1_0, 10},
        {LEVEL_1_B, 9},
        {LEVEL_1_1, 11},
        {LEVEL_1_2, 12},
        {LEVEL_1_3, 13},
        {LEVEL_2_0, 20},
        {LEVEL_2_1, 21},
        {LEVEL_2_2, 22},
        {LEVEL_3_0, 30},
        {LEVEL_3_1, 31},
        {LEVEL_3_2, 32},
        {LEVEL_4_0, 40},
        {LEVEL_4_1, 41},
        {LEVEL_4_2, 42},
        {LEVEL_5_0, 50},
        {LEVEL_5_1, 51},
        {LEVEL_5_2, 52},
    });

    SEncParamExt params = encoder_.GetEncoderOptionForTesting();

    const auto eProfileIdc = params.sSpatialLayers[0].uiProfileIdc;
    if (!kEProfileIdcToProfile.Contains(eProfileIdc)) {
      NOTREACHED() << "Failed to convert unknown EProfileIdc: " << eProfileIdc;
    }

    const auto eLevelIdc = params.sSpatialLayers[0].uiLevelIdc;
    if (!kELevelIdcToLevel.Contains(eLevelIdc)) {
      NOTREACHED() << "Failed to convert unknown ELevelIdc: " << eLevelIdc;
    }
    return {kEProfileIdcToProfile.find(eProfileIdc)->value,
            kELevelIdcToLevel.find(eLevelIdc)->value};
  }

  void OnEncodedVideo(
      const media::Muxer::VideoParameters& params,
      std::string encoded_data,
      std::string encoded_alpha,
      absl::optional<media::VideoEncoder::CodecDescription> codec_description,
      base::TimeTicks capture_timestamp,
      bool is_key_frame) {}

  const absl::optional<media::VideoCodecProfile> profile_;
  const absl::optional<uint8_t> level_;
  const uint32_t bitrate_;
  raw_ptr<media::MockVideoEncoderMetricsProvider, ExperimentalRenderer>
      mock_metrics_provider_;
  H264Encoder encoder_;
};

class H264EncoderParameterTest
    : public H264EncoderFixture,
      public ::testing::WithParamInterface<TestParam> {
 public:
  H264EncoderParameterTest()
      : H264EncoderFixture(GetParam().profile,
                           GetParam().level,
                           GetParam().bitrate) {}

  H264EncoderParameterTest(const H264EncoderParameterTest&) = delete;
  H264EncoderParameterTest& operator=(const H264EncoderParameterTest&) = delete;
};

TEST_P(H264EncoderParameterTest, CheckProfileLevel) {
  // The encoder will be initialized with specified parameters after encoded
  // first frame.
  EXPECT_CALL(
      *mock_metrics_provider_,
      MockInitialize(GetParam().profile.value_or(media::H264PROFILE_BASELINE),
                     gfx::Size(kFrameWidth, kFrameHeight),
                     /*hardware_video_encoder=*/false,
                     media::SVCScalabilityMode::kL1T1));
  EXPECT_CALL(*mock_metrics_provider_, MockIncrementEncodedFrameCount());
  EncodeFrame();

  auto profileLevel = GetProfileLevelForTesting();
  if (GetParam().profile)
    ASSERT_EQ(profileLevel.first, GetParam().profile);
  if (GetParam().level)
    ASSERT_EQ(profileLevel.second, GetParam().level);
}

INSTANTIATE_TEST_SUITE_P(All,
                         H264EncoderParameterTest,
                         testing::ValuesIn(kH264EncoderParameterTestParam));

}  // namespace blink
