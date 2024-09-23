// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/video_sender.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "components/openscreen_platform/task_runner.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "media/base/mock_filters.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_environment.h"
#include "media/cast/common/openscreen_conversion_helpers.h"
#include "media/cast/common/video_frame_factory.h"
#include "media/cast/constants.h"
#include "media/cast/logging/simple_event_subscriber.h"
#include "media/cast/test/fake_openscreen_clock.h"
#include "media/cast/test/fake_video_encode_accelerator_factory.h"
#include "media/cast/test/mock_openscreen_environment.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/video_utility.h"
#include "media/video/fake_video_encode_accelerator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/public/capture_recommendations.h"
#include "third_party/openscreen/src/cast/streaming/public/environment.h"
#include "third_party/openscreen/src/cast/streaming/public/sender.h"
#include "third_party/openscreen/src/cast/streaming/sender_packet_router.h"
#include "third_party/openscreen/src/platform/api/time.h"

namespace media::cast {

namespace {
static const uint8_t kPixelValue = 123;
static const int kWidth = 320;
static const int kHeight = 240;

using testing::_;
using testing::AtLeast;

void SaveOperationalStatus(OperationalStatus* out_status,
                           OperationalStatus in_status) {
  DVLOG(1) << "OperationalStatus transitioning from " << *out_status << " to "
           << in_status;
  *out_status = in_status;
}

void IgnorePlayoutDelayChanges(base::TimeDelta unused_playout_delay) {}

void IgnoreVideoCaptureFeedback(
    const media::VideoCaptureFeedback& unused_feedback) {}

int GetSuggestedVideoBitrate() {
  return openscreen::cast::kDefaultVideoMinBitRate;
}

}  // namespace

class VideoSenderTest : public ::testing::Test {
 public:
  VideoSenderTest(const VideoSenderTest&) = delete;
  VideoSenderTest& operator=(const VideoSenderTest&) = delete;

 protected:
  VideoSenderTest()
      : task_runner_(new FakeSingleThreadTaskRunner(&testing_clock_)),
        cast_environment_(new CastEnvironment(&testing_clock_,
                                              task_runner_,
                                              task_runner_,
                                              task_runner_)),
        openscreen_task_runner_(task_runner_),
        vea_factory_(task_runner_) {
    FakeOpenscreenClock::SetTickClock(&testing_clock_);
    testing_clock_.Advance(base::TimeTicks::Now() - base::TimeTicks());
    mock_openscreen_environment_ = std::make_unique<MockOpenscreenEnvironment>(
        &FakeOpenscreenClock::now, openscreen_task_runner_);
    openscreen_packet_router_ =
        std::make_unique<openscreen::cast::SenderPacketRouter>(
            *mock_openscreen_environment_);
    vea_factory_.SetAutoRespond(true);
    last_pixel_value_ = kPixelValue;
  }

  ~VideoSenderTest() override { FakeOpenscreenClock::ClearTickClock(); }

  void TearDown() final {
    // Video encoders owned by the VideoSender are deleted asynchronously.
    // Delete the VideoSender here and then run any posted deletion tasks.
    openscreen_video_sender_ = nullptr;
    video_sender_.reset();
    task_runner_->RunTasks();
  }

  // If |external| is true then external video encoder (VEA) is used.
  // |expect_init_success| is true if initialization is expected to succeed.
  void InitEncoder(bool external, bool expect_init_success) {
    FrameSenderConfig video_config = GetDefaultVideoSenderConfig();
    video_config.use_hardware_encoder = external;

    openscreen::cast::SessionConfig openscreen_video_config =
        ToOpenscreenSessionConfig(video_config, /* is_pli_enabled= */ true);

    ASSERT_EQ(operational_status_, STATUS_UNINITIALIZED);

    if (external) {
      vea_factory_.SetInitializationWillSucceed(expect_init_success);
    }

    auto openscreen_video_sender = std::make_unique<openscreen::cast::Sender>(
        *mock_openscreen_environment_, *openscreen_packet_router_,
        openscreen_video_config, openscreen::cast::RtpPayloadType::kVideoVp8);
    openscreen_video_sender_ = openscreen_video_sender.get();

    video_sender_ = std::make_unique<VideoSender>(
        cast_environment_, video_config,
        base::BindRepeating(&SaveOperationalStatus, &operational_status_),
        base::BindRepeating(
            &FakeVideoEncodeAcceleratorFactory::CreateVideoEncodeAccelerator,
            base::Unretained(&vea_factory_)),
        std::move(openscreen_video_sender),
        std::make_unique<media::MockVideoEncoderMetricsProvider>(),
        base::BindRepeating(&IgnorePlayoutDelayChanges),
        base::BindRepeating(&IgnoreVideoCaptureFeedback),
        base::BindRepeating(&GetSuggestedVideoBitrate));

    task_runner_->RunTasks();
  }

  scoped_refptr<media::VideoFrame> GetNewVideoFrame() {
    if (first_frame_timestamp_.is_null())
      first_frame_timestamp_ = testing_clock_.NowTicks();
    gfx::Size size(kWidth, kHeight);
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::CreateFrame(
            PIXEL_FORMAT_I420, size, gfx::Rect(size), size,
            testing_clock_.NowTicks() - first_frame_timestamp_);
    PopulateVideoFrame(video_frame.get(), last_pixel_value_++);
    return video_frame;
  }

  void RunTasks(int during_ms) {
    task_runner_->Sleep(base::Milliseconds(during_ms));
  }

  base::SimpleTestTickClock testing_clock_;
  const scoped_refptr<FakeSingleThreadTaskRunner> task_runner_;
  const scoped_refptr<CastEnvironment> cast_environment_;
  // openscreen::Sender related classes.
  openscreen_platform::TaskRunner openscreen_task_runner_;
  std::unique_ptr<media::cast::MockOpenscreenEnvironment>
      mock_openscreen_environment_;
  std::unique_ptr<openscreen::cast::SenderPacketRouter>
      openscreen_packet_router_;
  OperationalStatus operational_status_ = STATUS_UNINITIALIZED;
  FakeVideoEncodeAcceleratorFactory vea_factory_;
  int last_pixel_value_;
  base::TimeTicks first_frame_timestamp_;
  std::unique_ptr<VideoSender> video_sender_;
  // Unowned pointer to the openscreen::cast::Sender.
  raw_ptr<openscreen::cast::Sender> openscreen_video_sender_;
};

TEST_F(VideoSenderTest, BuiltInEncoder) {
  InitEncoder(false, true);
  ASSERT_EQ(STATUS_INITIALIZED, operational_status_);

  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();

  const base::TimeTicks reference_time = testing_clock_.NowTicks();
  video_sender_->InsertRawVideoFrame(video_frame, reference_time);

  task_runner_->RunTasks();
  EXPECT_EQ(1, openscreen_video_sender_->GetInFlightFrameCount());
}

TEST_F(VideoSenderTest, ExternalEncoder) {
  InitEncoder(true, true);
  ASSERT_EQ(STATUS_INITIALIZED, operational_status_);

  // The SizeAdaptableExternalVideoEncoder initially reports STATUS_INITIALIZED
  // so that frames will be sent to it.  Therefore, no encoder activity should
  // have occurred at this point.  Send a frame to spurn creation of the
  // underlying ExternalVideoEncoder instance.
  if (vea_factory_.vea_response_count() == 0) {
    video_sender_->InsertRawVideoFrame(GetNewVideoFrame(),
                                       testing_clock_.NowTicks());
    task_runner_->RunTasks();
  }
  ASSERT_EQ(STATUS_INITIALIZED, operational_status_);
  RunTasks(33);

  // VideoSender created an encoder for 1280x720 frames, in order to provide the
  // INITIALIZED status.
  EXPECT_EQ(1, vea_factory_.vea_response_count());

  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();

  for (int i = 0; i < 3; ++i) {
    const base::TimeTicks reference_time = testing_clock_.NowTicks();
    video_sender_->InsertRawVideoFrame(video_frame, reference_time);
    RunTasks(33);
    // VideoSender re-created the encoder for the 320x240 frames we're
    // providing.
    EXPECT_EQ(1, vea_factory_.vea_response_count());
  }

  // NOTE: Must delete video_sender_ before test exits to avoid dangling pointer
  // issues; root cause is unclear
  openscreen_video_sender_ = nullptr;
  video_sender_.reset();
  task_runner_->RunTasks();
  EXPECT_EQ(1, vea_factory_.vea_response_count());
}

TEST_F(VideoSenderTest, ExternalEncoderInitFails) {
  InitEncoder(true, false);

  // The SizeAdaptableExternalVideoEncoder initially reports STATUS_INITIALIZED
  // so that frames will be sent to it.  Send a frame to spurn creation of the
  // underlying ExternalVideoEncoder instance, which should result in failure.
  if (operational_status_ == STATUS_INITIALIZED ||
      operational_status_ == STATUS_CODEC_REINIT_PENDING) {
    video_sender_->InsertRawVideoFrame(GetNewVideoFrame(),
                                       testing_clock_.NowTicks());
    task_runner_->RunTasks();
  }
  EXPECT_EQ(STATUS_CODEC_INIT_FAILED, operational_status_);

  // NOTE: Must delete video_sender_ before test exits to avoid dangling pointer
  // issues; root cause is unclear
  openscreen_video_sender_ = nullptr;
  video_sender_.reset();
  task_runner_->RunTasks();
}

TEST_F(VideoSenderTest, CheckVideoFrameFactoryIsNull) {
  InitEncoder(false, true);
  ASSERT_EQ(STATUS_INITIALIZED, operational_status_);
  EXPECT_EQ(nullptr, video_sender_->CreateVideoFrameFactory().get());
}

}  // namespace media::cast
