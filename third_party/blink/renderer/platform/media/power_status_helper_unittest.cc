// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/power_status_helper.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "media/base/pipeline_metadata.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/device/public/mojom/battery_status.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using ::testing::Bool;
using ::testing::Combine;
using ::testing::Values;

class PowerStatusHelperTest : public testing::Test {
 public:
  class MockBatteryMonitor : public device::mojom::blink::BatteryMonitor {
   public:
    MOCK_METHOD0(DidGetBatteryMonitor, void());
    MOCK_METHOD0(DidQueryNextStatus, void());
    MOCK_METHOD0(DidDisconnect, void());

    ~MockBatteryMonitor() override {
      // Mojo gets mad if we don't finish up outstanding callbacks.
      if (callback_)
        ProvidePowerUpdate(false, 0);
    }

    // device::mojom::blink::BatteryMonitor
    void QueryNextStatus(QueryNextStatusCallback callback) override {
      DidQueryNextStatus();
      callback_ = std::move(callback);
    }

    // Would be nice to use a MockCallback for this, but a move-only return type
    // doesn't seem to work.
    mojo::PendingRemote<device::mojom::blink::BatteryMonitor>
    GetBatteryMonitor() {
      DidGetBatteryMonitor();
      switch (remote_type_) {
        case RemoteType::kConnected:
        case RemoteType::kDisconnected: {
          auto pending = receiver_.BindNewPipeAndPassRemote();
          receiver_.set_disconnect_handler(base::BindOnce(
              &MockBatteryMonitor::DidDisconnect, base::Unretained(this)));
          if (remote_type_ == RemoteType::kDisconnected)
            receiver_.reset();
          base::RunLoop().RunUntilIdle();
          return pending;
        }
        case RemoteType::kEmpty:
          return mojo::PendingRemote<device::mojom::blink::BatteryMonitor>();
      }
    }

    // Would be nice if this were base::MockCallback, but move-only types don't
    // seem to work.
    PowerStatusHelper::CreateBatteryMonitorCB cb() {
      return base::BindRepeating(&MockBatteryMonitor::GetBatteryMonitor,
                                 base::Unretained(this));
    }

    // Provide a battery update via |callback_|.
    void ProvidePowerUpdate(bool is_charging, float current_level) {
      EXPECT_TRUE(callback_);
      device::mojom::blink::BatteryStatusPtr status =
          device::mojom::blink::BatteryStatus::New(is_charging,
                                                   /*charging_time=*/0,
                                                   /*discharging_time=*/0,
                                                   current_level);
      std::move(callback_).Run(std::move(status));
      base::RunLoop().RunUntilIdle();
    }

    mojo::Receiver<device::mojom::blink::BatteryMonitor> receiver_{this};

    // If false, then GetBatteryMonitor will not return a monitor.
    enum class RemoteType {
      // Provide a connected remote.
      kConnected,
      // Provide an empty PendingRemote
      kEmpty,
      // Provide a PendingRemote to a disconnected remote.
      kDisconnected
    };
    RemoteType remote_type_ = RemoteType::kConnected;

    // Most recently provided callback.
    QueryNextStatusCallback callback_;
  };

  void SetUp() override {
    helper_ = std::make_unique<PowerStatusHelper>(monitor_.cb());
  }

  // Set up |helper_| to be in a state that should record. Returns the bucket.
  // |alternate| just causes us to create a different recordable bucket.
  int MakeRecordable(bool alternate = false) {
    helper_->SetIsPlaying(true);
    media::PipelineMetadata metadata;
    metadata.has_video = true;
    metadata.video_decoder_config = media::VideoDecoderConfig(
        media::VideoCodec::kH264, media::H264PROFILE_MAIN,
        media::VideoDecoderConfig::AlphaMode::kIsOpaque,
        media::VideoColorSpace(), media::VideoTransformation(),
        gfx::Size(0, 0),        /* coded_size */
        gfx::Rect(0, 0),        /* visible rect */
        gfx::Size(640, 360),    /* natural size */
        std::vector<uint8_t>(), /* extra_data */
        media::EncryptionScheme::kUnencrypted);
    helper_->SetMetadata(metadata);
    helper_->SetAverageFrameRate(60);
    // Use |alternate| to set fullscreen state, since that should still be
    // recordable but in a different bucket.
    helper_->SetIsFullscreen(alternate);
    base::RunLoop().RunUntilIdle();
    helper_->UpdatePowerExperimentState(true);
    base::RunLoop().RunUntilIdle();

    return PowerStatusHelper::kCodecBitsH264 |
           PowerStatusHelper::kResolution360p |
           PowerStatusHelper::kFrameRate60 |
           (alternate ? PowerStatusHelper::kFullScreenYes
                      : PowerStatusHelper::kFullScreenNo);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // Previous total histogram counts.  Note that we record the total in msec,
  // rather than as a TimeDelta, so that we round the same way as the helper.
  int total_battery_delta = 0;
  int total_time_delta = 0;  // msec

  MockBatteryMonitor monitor_;

  // Helper under test
  std::unique_ptr<PowerStatusHelper> helper_;

  base::HistogramTester histogram_tester_;
};

TEST_F(PowerStatusHelperTest, EmptyPendingRemoteIsOkay) {
  // Enable power monitoring, but have the callback fail to provide a remote.
  // This should be handled gracefully.

  // Ask |monitor_| not to provide a remote, and expect that |helper_| asks.
  monitor_.remote_type_ = MockBatteryMonitor::RemoteType::kEmpty;
  EXPECT_CALL(monitor_, DidGetBatteryMonitor()).Times(1);
  MakeRecordable();
}

TEST_F(PowerStatusHelperTest, UnboundPendingRemoteIsOkay) {
  // TODO: this doesn't run the "is bound" part.  maybe we should just delete
  // the "is bound" part, or switch to a disconnection handler, etc.
  monitor_.remote_type_ = MockBatteryMonitor::RemoteType::kDisconnected;
  EXPECT_CALL(monitor_, DidGetBatteryMonitor()).Times(1);
  MakeRecordable();
}

TEST_F(PowerStatusHelperTest, BasicReportingWithFractionalAmounts) {
  // Send three power updates, and verify that an update is called for the
  // last two.  The update should be fractional, so that some of it is rolled
  // over to the next call.
  EXPECT_CALL(monitor_, DidGetBatteryMonitor()).Times(1);
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  MakeRecordable();

  const float baseline_level = 0.9;

  // This should be the baseline.
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  monitor_.ProvidePowerUpdate(false, baseline_level);
}

TEST_F(PowerStatusHelperTest, ChargingResetsBaseline) {
  // Send some power updates, then send an update that's marked as 'charging'.
  // Make sure that the baseline resets.
  EXPECT_CALL(monitor_, DidGetBatteryMonitor()).Times(1);
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  MakeRecordable();

  const float fake_baseline_level = 0.95;
  const float baseline_level = 0.9;
  const float second_level = baseline_level - 0.10;

  // Send the fake baseline.
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  monitor_.ProvidePowerUpdate(false, fake_baseline_level);

  // Send an update that's marked as charging.
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  monitor_.ProvidePowerUpdate(true, second_level);

  // This should be the correct baseline.
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  monitor_.ProvidePowerUpdate(false, baseline_level);
}

TEST_F(PowerStatusHelperTest, ExperimentStateStopsRecording) {
  // Verify that stopping the power experiment stops recording.
  EXPECT_CALL(monitor_, DidGetBatteryMonitor()).Times(1);
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  MakeRecordable();

  EXPECT_CALL(monitor_, DidDisconnect()).Times(1);
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(0);
  helper_->UpdatePowerExperimentState(false);
  base::RunLoop().RunUntilIdle();

  // Call the callback to make sure nothing bad happens.  It should be ignored,
  // since it shouldn't use battery updates after the experiment stops.
  monitor_.ProvidePowerUpdate(false, 1.0);
}

TEST_F(PowerStatusHelperTest, ChangingBucketsWorks) {
  // Switch buckets mid-recording, and make sure that we get a new bucket and
  // use a new baseline.
  EXPECT_CALL(monitor_, DidGetBatteryMonitor()).Times(1);
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  auto first_bucket = MakeRecordable(false);

  const float fake_baseline_level = 0.95;
  const float baseline_level = 0.9;

  // Send the fake baseline.
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  monitor_.ProvidePowerUpdate(false, fake_baseline_level);

  // Switch buckets.
  auto second_bucket = MakeRecordable(true);
  ASSERT_NE(first_bucket, second_bucket);

  // This should be the correct baseline.
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  monitor_.ProvidePowerUpdate(false, baseline_level);
}

TEST_F(PowerStatusHelperTest, UnbucketedVideoStopsRecording) {
  // If we switch to video that doesn't have a bucket, then recording should
  // stop too.
  EXPECT_CALL(monitor_, DidGetBatteryMonitor()).Times(1);
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  MakeRecordable();

  // Should disconnect when we send bad params.
  EXPECT_CALL(monitor_, DidDisconnect()).Times(1);
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(0);
  helper_->SetIsPlaying(false);
  base::RunLoop().RunUntilIdle();
}

TEST_F(PowerStatusHelperTest, UnbucketedFrameRateStopsRecording) {
  // If we switch to an unbucketed frame rate, then it should stop recording.
  EXPECT_CALL(monitor_, DidGetBatteryMonitor()).Times(1);
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  MakeRecordable();

  // Should disconnect when we send bad params.
  EXPECT_CALL(monitor_, DidDisconnect()).Times(1);
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(0);
  helper_->SetAverageFrameRate({});
  base::RunLoop().RunUntilIdle();
}

using PlaybackParamsTuple = std::tuple<bool,                    /* is_playing */
                                       bool,                    /* has_video */
                                       PowerStatusHelper::Bits, /* codec */
                                       PowerStatusHelper::Bits, /* resolution */
                                       PowerStatusHelper::Bits, /* frame rate */
                                       PowerStatusHelper::Bits /* full screen */
                                       >;

class PowerStatusHelperBucketTest
    : public testing::TestWithParam<PlaybackParamsTuple> {
 public:
  std::optional<int> BucketFor(bool is_playing,
                               bool has_video,
                               media::VideoCodec codec,
                               media::VideoCodecProfile profile,
                               gfx::Size coded_size,
                               bool is_fullscreen,
                               std::optional<int> average_fps) {
    return PowerStatusHelper::BucketFor(is_playing, has_video, codec, profile,
                                        coded_size, is_fullscreen, average_fps);
  }
};

TEST_P(PowerStatusHelperBucketTest, TestBucket) {
  // Construct a params that should end up in the bucket specified by the test
  // parameter, if one exists.
  bool expect_bucket = true;

  bool is_playing = std::get<0>(GetParam());
  bool has_video = std::get<1>(GetParam());

  // We must be playing video to get a bucket.
  if (!is_playing || !has_video)
    expect_bucket = false;

  auto codec_bits = std::get<2>(GetParam());
  media::VideoCodec codec;
  media::VideoCodecProfile profile;
  if (codec_bits == PowerStatusHelper::Bits::kCodecBitsH264) {
    codec = media::VideoCodec::kH264;
    profile = media::H264PROFILE_MAIN;
  } else if (codec_bits == PowerStatusHelper::Bits::kCodecBitsVP9Profile0) {
    codec = media::VideoCodec::kVP9;
    profile = media::VP9PROFILE_PROFILE0;
  } else if (codec_bits == PowerStatusHelper::Bits::kCodecBitsVP9Profile2) {
    codec = media::VideoCodec::kVP9;
    profile = media::VP9PROFILE_PROFILE2;
  } else {
    // Some unsupported codec.
    codec = media::VideoCodec::kVP8;
    profile = media::VIDEO_CODEC_PROFILE_UNKNOWN;
    expect_bucket = false;
  }

  auto res = std::get<3>(GetParam());
  gfx::Size coded_size;
  if (res == PowerStatusHelper::Bits::kResolution360p) {
    coded_size = gfx::Size(640, 360);
  } else if (res == PowerStatusHelper::Bits::kResolution720p) {
    coded_size = gfx::Size(1280, 720);
  } else if (res == PowerStatusHelper::Bits::kResolution1080p) {
    coded_size = gfx::Size(1920, 1080);
  } else {
    coded_size = gfx::Size(1234, 5678);
    expect_bucket = false;
  }

  auto fps = std::get<4>(GetParam());
  std::optional<int> average_fps;
  if (fps == PowerStatusHelper::Bits::kFrameRate30) {
    average_fps = 30;
  } else if (fps == PowerStatusHelper::Bits::kFrameRate60) {
    average_fps = 60;
  } else {
    average_fps = 90;
    expect_bucket = false;
  }

  bool is_fullscreen =
      (std::get<5>(GetParam()) == PowerStatusHelper::Bits::kFullScreenYes);

  auto bucket = BucketFor(is_playing, has_video, codec, profile, coded_size,
                          is_fullscreen, average_fps);
  if (!expect_bucket) {
    EXPECT_FALSE(bucket);
  } else {
    EXPECT_EQ(*bucket, std::get<2>(GetParam()) | std::get<3>(GetParam()) |
                           std::get<4>(GetParam()) | std::get<5>(GetParam()));
  }
}

// Instantiate all valid combinations, plus some that aren't.
INSTANTIATE_TEST_SUITE_P(
    All,
    PowerStatusHelperBucketTest,
    Combine(Bool(),
            Bool(),
            Values(PowerStatusHelper::Bits::kCodecBitsH264,
                   PowerStatusHelper::Bits::kCodecBitsVP9Profile0,
                   PowerStatusHelper::Bits::kCodecBitsVP9Profile2,
                   PowerStatusHelper::Bits::kNotAValidBitForTesting),
            Values(PowerStatusHelper::Bits::kResolution360p,
                   PowerStatusHelper::Bits::kResolution720p,
                   PowerStatusHelper::Bits::kResolution1080p,
                   PowerStatusHelper::Bits::kNotAValidBitForTesting),
            Values(PowerStatusHelper::Bits::kFrameRate30,
                   PowerStatusHelper::Bits::kFrameRate60,
                   PowerStatusHelper::Bits::kNotAValidBitForTesting),
            Values(PowerStatusHelper::Bits::kFullScreenNo,
                   PowerStatusHelper::Bits::kFullScreenYes)));

}  // namespace blink
