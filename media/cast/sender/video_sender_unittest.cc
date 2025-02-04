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
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/openscreen_platform/task_runner.h"
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "media/base/media_switches.h"
#include "media/base/mock_filters.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_environment.h"
#include "media/cast/common/openscreen_conversion_helpers.h"
#include "media/cast/constants.h"
#include "media/cast/test/fake_openscreen_clock.h"
#include "media/cast/test/fake_video_encode_accelerator_factory.h"
#include "media/cast/test/mock_openscreen_environment.h"
#include "media/cast/test/test_with_cast_environment.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/video_utility.h"
#include "media/video/fake_video_encode_accelerator.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/public/capture_recommendations.h"
#include "third_party/openscreen/src/cast/streaming/public/environment.h"
#include "third_party/openscreen/src/cast/streaming/public/sender.h"
#include "third_party/openscreen/src/cast/streaming/sender_packet_router.h"
#include "third_party/openscreen/src/platform/api/time.h"

namespace media::cast {

namespace {

constexpr uint8_t kPixelValue = 123;
constexpr int kWidth = 320;
constexpr int kHeight = 240;

constexpr gfx::Size kDefaultSize(1920, 1080);

static const std::vector<media::VideoEncodeAccelerator::SupportedProfile>
    kDefaultSupportedProfiles = {
        media::VideoEncodeAccelerator::SupportedProfile(H264PROFILE_MAIN,
                                                        kDefaultSize),
        media::VideoEncodeAccelerator::SupportedProfile(VP8PROFILE_ANY,
                                                        kDefaultSize),
        media::VideoEncodeAccelerator::SupportedProfile(VP9PROFILE_PROFILE0,
                                                        kDefaultSize),
        media::VideoEncodeAccelerator::SupportedProfile(AV1PROFILE_PROFILE_MAIN,
                                                        kDefaultSize)};

using testing::_;
using testing::AtLeast;

void SaveOperationalStatus(std::vector<OperationalStatus>* statuses,
                           OperationalStatus in_status) {
  DVLOG(1) << "OperationalStatus transitioning to " << in_status;
  statuses->push_back(in_status);
}

void IgnorePlayoutDelayChanges(base::TimeDelta unused_playout_delay) {}

int GetVideoNetworkBandwidth() {
  return openscreen::cast::kDefaultVideoMinBitRate;
}

}  // namespace

class VideoSenderTest : public ::testing::TestWithParam<bool>,
                        public WithCastEnvironment {
 public:
  VideoSenderTest(const VideoSenderTest&) = delete;
  VideoSenderTest& operator=(const VideoSenderTest&) = delete;

 protected:
  VideoSenderTest() {
    openscreen_task_runner_ = std::make_unique<openscreen_platform::TaskRunner>(
        GetMainThreadTaskRunner());
    accelerator_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::SingleThreadTaskRunnerThreadMode::DEDICATED);


    vea_factory_ = std::make_unique<FakeVideoEncodeAcceleratorFactory>(
        accelerator_task_runner_);

    FakeOpenscreenClock::SetTickClock(GetMockTickClock());
    mock_openscreen_environment_ = std::make_unique<MockOpenscreenEnvironment>(
        &FakeOpenscreenClock::now, *openscreen_task_runner_);
    openscreen_packet_router_ =
        std::make_unique<openscreen::cast::SenderPacketRouter>(
            *mock_openscreen_environment_);
    vea_factory_->SetAutoRespond(true);
    last_pixel_value_ = kPixelValue;
    feature_list_.InitWithFeatureState(kCastStreamingMediaVideoEncoder,
                                       GetParam());
  }

  ~VideoSenderTest() override {
    // Video encoders owned by the VideoSender are deleted asynchronously.
    // Delete the VideoSender here and then run any posted deletion tasks.
    openscreen_video_sender_ = nullptr;
    video_sender_.reset();
    RunTasksAndAdvanceClock();
    FakeOpenscreenClock::ClearTickClock();
  }

  void RunTasksAndAdvanceClock(base::TimeDelta clock_delta = {}) {
    AdvanceClock(clock_delta);

    accelerator_task_runner_->PostTask(FROM_HERE, QuitClosure());
    RunUntilQuit();
    GetMainThreadTaskRunner()->PostTask(FROM_HERE, QuitClosure());
    RunUntilQuit();
  }

  // Can be used to be notified when video capture feedback is created. This is
  // only done when a frame is successfully encoded and enqueued into the
  // Open Screen frame sender.
  void SetVideoCaptureFeedbackClosure(base::OnceClosure closure) {
    feedback_closure_ = std::move(closure);
  }

  void HandleVideoCaptureFeedback(const media::VideoCaptureFeedback&) {
    if (feedback_closure_) {
      std::move(feedback_closure_).Run();
    }
  }

  // If |external| is true then external video encoder (VEA) is used.
  // |expect_init_success| is true if initialization is expected to succeed.
  void InitEncoder(bool external, bool expect_init_success) {
    FrameSenderConfig video_config = GetDefaultVideoSenderConfig();
    video_config.use_hardware_encoder = external;

    openscreen::cast::SessionConfig openscreen_video_config =
        ToOpenscreenSessionConfig(video_config, /* is_pli_enabled= */ true);

    ASSERT_TRUE(status_changes_.empty());

    if (external) {
      vea_factory_->SetInitializationWillSucceed(expect_init_success);
    }

    auto openscreen_video_sender = std::make_unique<openscreen::cast::Sender>(
        *mock_openscreen_environment_, *openscreen_packet_router_,
        openscreen_video_config, openscreen::cast::RtpPayloadType::kVideoVp8);
    openscreen_video_sender_ = openscreen_video_sender.get();

    if (external) {
      sii_ = base::MakeRefCounted<gpu::TestSharedImageInterface>();
      sii_->UseTestGMBInSharedImageCreationWithBufferUsage();
      mock_gpu_factories_ =
          std::make_unique<MockGpuVideoAcceleratorFactories>(sii_.get());
      EXPECT_CALL(*mock_gpu_factories_, GetTaskRunner())
          .WillRepeatedly(testing::Return(accelerator_task_runner_));
      EXPECT_CALL(*mock_gpu_factories_, DoCreateVideoEncodeAccelerator())
          .WillRepeatedly(testing::Invoke([&]() {
            return vea_factory_->CreateVideoEncodeAcceleratorSync().release();
          }));
      EXPECT_CALL(*mock_gpu_factories_,
                  GetVideoEncodeAcceleratorSupportedProfiles())
          .WillRepeatedly(
              testing::Invoke([&]() { return kDefaultSupportedProfiles; }));
    }

    video_sender_ = std::make_unique<VideoSender>(
        cast_environment(), video_config,
        base::BindRepeating(&SaveOperationalStatus, &status_changes_),
        base::BindRepeating(
            &FakeVideoEncodeAcceleratorFactory::CreateVideoEncodeAccelerator,
            base::Unretained(vea_factory_.get())),
        std::move(openscreen_video_sender),
        std::make_unique<media::MockVideoEncoderMetricsProvider>(),
        base::BindRepeating(&IgnorePlayoutDelayChanges),
        base::BindRepeating(&VideoSenderTest::HandleVideoCaptureFeedback,
                            base::Unretained(this)),
        base::BindRepeating(&GetVideoNetworkBandwidth),
        mock_gpu_factories_.get());

    RunTasksAndAdvanceClock();
  }

  scoped_refptr<media::VideoFrame> GetNewVideoFrame() {
    if (first_frame_timestamp_.is_null()) {
      first_frame_timestamp_ = NowTicks();
    }
    constexpr gfx::Size kSize(kWidth, kHeight);
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::CreateFrame(PIXEL_FORMAT_I420, kSize,
                                       gfx::Rect(kSize), kSize,
                                       NowTicks() - first_frame_timestamp_);
    PopulateVideoFrame(video_frame.get(), last_pixel_value_++);
    return video_frame;
  }

  scoped_refptr<base::SingleThreadTaskRunner> accelerator_task_runner_;

  // openscreen::Sender related classes.
  std::unique_ptr<openscreen_platform::TaskRunner> openscreen_task_runner_;
  std::unique_ptr<media::cast::MockOpenscreenEnvironment>
      mock_openscreen_environment_;
  std::unique_ptr<openscreen::cast::SenderPacketRouter>
      openscreen_packet_router_;
  std::vector<OperationalStatus> status_changes_;
  std::unique_ptr<FakeVideoEncodeAcceleratorFactory> vea_factory_;
  int last_pixel_value_;
  base::TimeTicks first_frame_timestamp_;
  std::unique_ptr<VideoSender> video_sender_;
  base::OnceClosure feedback_closure_;
  scoped_refptr<gpu::TestSharedImageInterface> sii_;
  std::unique_ptr<MockGpuVideoAcceleratorFactories> mock_gpu_factories_;

  // Unowned pointer to the openscreen::cast::Sender.
  raw_ptr<openscreen::cast::Sender> openscreen_video_sender_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(VideoSenderTest, BuiltInEncoder) {
  InitEncoder(false, true);
  ASSERT_EQ(STATUS_INITIALIZED, status_changes_.front());

  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();
  video_sender_->InsertRawVideoFrame(video_frame, NowTicks());

  SetVideoCaptureFeedbackClosure(task_environment().QuitClosure());
  RunUntilQuit();
}

TEST_P(VideoSenderTest, ExternalEncoder) {
  InitEncoder(true, true);
  ASSERT_EQ(STATUS_INITIALIZED, status_changes_.front());

  // The SizeAdaptableExternalVideoEncoder initially reports STATUS_INITIALIZED
  // so that frames will be sent to it.  Therefore, no encoder activity should
  // have occurred at this point.  Send a frame to spurn creation of the
  // underlying ExternalVideoEncoder instance.
  if (vea_factory_->vea_response_count() == 0) {
    video_sender_->InsertRawVideoFrame(GetNewVideoFrame(), NowTicks());
    RunTasksAndAdvanceClock();
  }
  ASSERT_EQ(STATUS_INITIALIZED, status_changes_.front());
  RunTasksAndAdvanceClock(base::Milliseconds(33));

  // VideoSender created an encoder for 1280x720 frames, in order to provide the
  // INITIALIZED status.
  EXPECT_EQ(1, vea_factory_->vea_response_count());

  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();

  for (int i = 0; i < 3; ++i) {
    video_sender_->InsertRawVideoFrame(video_frame, NowTicks());
    RunTasksAndAdvanceClock(base::Milliseconds(33));
    // VideoSender re-created the encoder for the 320x240 frames we're
    // providing.
    EXPECT_EQ(1, vea_factory_->vea_response_count());
  }

  // NOTE: Must delete video_sender_ before test exits to avoid dangling pointer
  // issues; root cause is unclear
  openscreen_video_sender_ = nullptr;
  video_sender_.reset();
  RunTasksAndAdvanceClock();
  EXPECT_EQ(1, vea_factory_->vea_response_count());
}

TEST_P(VideoSenderTest, ExternalEncoderInitFails) {
  InitEncoder(true, false);
  EXPECT_EQ(STATUS_INITIALIZED, status_changes_.front());

  // Send a frame to spurn creation of the underlying ExternalVideoEncoder
  // instance, which should result in failure.
  video_sender_->InsertRawVideoFrame(GetNewVideoFrame(), NowTicks());
  RunTasksAndAdvanceClock();

  EXPECT_NE(std::ranges::find(status_changes_, STATUS_CODEC_INIT_FAILED),
            status_changes_.end());

  // NOTE: Must delete video_sender_ before test exits to avoid dangling pointer
  // issues; root cause is unclear
  openscreen_video_sender_ = nullptr;
  video_sender_.reset();
  RunTasksAndAdvanceClock();
}

INSTANTIATE_TEST_SUITE_P(All, VideoSenderTest, ::testing::Bool());

}  // namespace media::cast
