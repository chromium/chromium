// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/video_sender.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/heap_array.h"
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
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "media/base/media_switches.h"
#include "media/base/mock_filters.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_environment.h"
#include "media/cast/common/openscreen_conversion_helpers.h"
#include "media/cast/constants.h"
#include "media/cast/encoding/video_encoder.h"
#include "media/cast/test/fake_video_encode_accelerator_factory.h"
#include "media/cast/test/mock_video_encoder.h"
#include "media/cast/test/openscreen_test_helpers.h"
#include "media/cast/test/test_with_cast_environment.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/video_utility.h"
#include "media/video/fake_video_encode_accelerator.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/public/sender.h"

using testing::Contains;

namespace media::cast {

namespace {

constexpr uint8_t kPixelValue = 123;
constexpr int kWidth = 320;
constexpr int kHeight = 240;

constexpr gfx::Size kDefaultSize(1920, 1080);

static const std::vector<VideoEncodeAccelerator::SupportedProfile>
    kDefaultSupportedProfiles = {
        VideoEncodeAccelerator::SupportedProfile(H264PROFILE_MAIN,
                                                 kDefaultSize),
        VideoEncodeAccelerator::SupportedProfile(VP8PROFILE_ANY, kDefaultSize),
        VideoEncodeAccelerator::SupportedProfile(VP9PROFILE_PROFILE0,
                                                 kDefaultSize),
        VideoEncodeAccelerator::SupportedProfile(AV1PROFILE_PROFILE_MAIN,
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
  VideoSenderTest(VideoSenderTest&&) = delete;
  VideoSenderTest& operator=(const VideoSenderTest&) = delete;
  VideoSenderTest& operator=(VideoSenderTest&&) = delete;

 protected:
  VideoSenderTest() {
    accelerator_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::SingleThreadTaskRunnerThreadMode::DEDICATED);


    vea_factory_ = std::make_unique<FakeVideoEncodeAcceleratorFactory>(
        accelerator_task_runner_);

    vea_factory_->SetAutoRespond(true);
    last_pixel_value_ = kPixelValue;
    feature_list_.InitWithFeatureState(kCastStreamingMediaVideoEncoder,
                                       GetParam());
  }

  ~VideoSenderTest() override {
    // Ensure the mock encoder is deleted first, to avoid a dangling raw_ptr.
    mock_encoder_ = nullptr;
    video_sender_.reset();

    // Run any posted deletion tasks. This may take multiple quit closures for
    // more complex encoder cases, such as VideoEncodeAcceleratorAdapters.
    RunTasksAndAdvanceClock();
    RunTasksAndAdvanceClock();
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

  void HandleVideoCaptureFeedback(const VideoCaptureFeedback&) {
    if (feedback_closure_) {
      std::move(feedback_closure_).Run();
    }
  }

  void SetVeaFactoryInitializationWillSucceed(bool will_succeed) {
    vea_factory_->SetInitializationWillSucceed(will_succeed);
  }

  int VeaResponseCount() const { return vea_factory_->vea_response_count(); }

  // Used to represent the different types of encoders that we wish to test
  // the VideoSender with.
  enum class EncoderType {
    // Use a software encoder configuration.
    kSoftware,

    // Use a hardware encoder configuration, which requires setting up some
    // additional GPU resources.
    kHardware,

    // Use a mock encoder configuration, which requires adding expectations for
    // behavior.
    kMock
  };

  // Create a VideoSender instance using an encoder of type `encoder_type`.
  void CreateSender(EncoderType encoder_type) {
    FrameSenderConfig video_config = GetDefaultVideoSenderConfig();
    video_config.use_hardware_encoder = encoder_type == EncoderType::kHardware;

    EXPECT_TRUE(status_changes_.empty());

    test_senders_ =
        std::make_unique<OpenscreenTestSenders>(OpenscreenTestSenders::Config(
            GetMainThreadTaskRunner(), GetMockTickClock(), std::nullopt,
            openscreen::cast::RtpPayloadType::kVideoVp8, std::nullopt,
            video_config));

    if (encoder_type == EncoderType::kHardware) {
      sii_ = base::MakeRefCounted<gpu::TestSharedImageInterface>();
      mock_gpu_factories_ =
          std::make_unique<MockGpuVideoAcceleratorFactories>(sii_.get());
      EXPECT_CALL(*mock_gpu_factories_, GetTaskRunner())
          .WillRepeatedly(testing::Return(accelerator_task_runner_));
      EXPECT_CALL(*mock_gpu_factories_, DoCreateVideoEncodeAccelerator())
          .WillRepeatedly([&]() {
            return vea_factory_->CreateVideoEncodeAcceleratorSync().release();
          });
      EXPECT_CALL(*mock_gpu_factories_,
                  GetVideoEncodeAcceleratorSupportedProfiles())
          .WillRepeatedly([&]() { return kDefaultSupportedProfiles; });
    }

    std::unique_ptr<VideoEncoder> video_encoder;
    if (encoder_type == EncoderType::kMock) {
      auto mock_encoder = std::make_unique<MockVideoEncoder>();
      mock_encoder_ = mock_encoder.get();
      video_encoder = std::move(mock_encoder);
    } else {
      video_encoder = VideoEncoder::Create(
          cast_environment(), video_config,
          std::make_unique<MockVideoEncoderMetricsProvider>(),

          base::BindRepeating(&SaveOperationalStatus, &status_changes_),
          base::BindRepeating(
              &FakeVideoEncodeAcceleratorFactory::CreateVideoEncodeAccelerator,
              base::Unretained(vea_factory_.get())),
          mock_gpu_factories_.get());
    }

    video_sender_ = std::make_unique<VideoSender>(
        std::move(video_encoder), cast_environment(), video_config,
        std::move(test_senders_->video_sender),
        base::BindRepeating(&IgnorePlayoutDelayChanges),
        base::BindRepeating(&VideoSenderTest::HandleVideoCaptureFeedback,
                            base::Unretained(this)),
        base::BindRepeating(&GetVideoNetworkBandwidth));

    RunTasksAndAdvanceClock();
  }

  scoped_refptr<VideoFrame> GetNewVideoFrame() {
    if (first_frame_timestamp_.is_null()) {
      first_frame_timestamp_ = NowTicks();
    }
    constexpr gfx::Size kSize(kWidth, kHeight);
    scoped_refptr<VideoFrame> video_frame =
        VideoFrame::CreateFrame(PIXEL_FORMAT_I420, kSize, gfx::Rect(kSize),
                                kSize, NowTicks() - first_frame_timestamp_);
    PopulateVideoFrame(video_frame.get(), last_pixel_value_++);
    return video_frame;
  }

  const std::vector<OperationalStatus>& status_changes() const {
    return status_changes_;
  }

  MockVideoEncoder& mock_encoder() {
    EXPECT_TRUE(mock_encoder_);
    return *mock_encoder_;
  }

  VideoSender& video_sender() {
    EXPECT_TRUE(video_sender_);
    return *video_sender_;
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> accelerator_task_runner_;

  std::unique_ptr<OpenscreenTestSenders> test_senders_;
  std::vector<OperationalStatus> status_changes_;
  std::unique_ptr<FakeVideoEncodeAcceleratorFactory> vea_factory_;
  int last_pixel_value_;
  base::TimeTicks first_frame_timestamp_;
  base::OnceClosure feedback_closure_;
  scoped_refptr<gpu::TestSharedImageInterface> sii_;
  std::unique_ptr<MockGpuVideoAcceleratorFactories> mock_gpu_factories_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<VideoSender> video_sender_;
  raw_ptr<MockVideoEncoder> mock_encoder_ = nullptr;
};

TEST_P(VideoSenderTest, BuiltInEncoder) {
  CreateSender(EncoderType::kSoftware);
  ASSERT_EQ(STATUS_INITIALIZED, status_changes().front());

  scoped_refptr<VideoFrame> video_frame = GetNewVideoFrame();
  video_sender().InsertRawVideoFrame(video_frame, NowTicks());

  SetVideoCaptureFeedbackClosure(task_environment().QuitClosure());
  RunUntilQuit();
}

TEST_P(VideoSenderTest, MockEncoderGoldenCase) {
  CreateSender(EncoderType::kMock);

  VideoEncoder::FrameEncodedCallback callback;
  EXPECT_CALL(mock_encoder(), GenerateKeyFrame());
  EXPECT_CALL(mock_encoder(), SetBitRate(5000000));
  EXPECT_CALL(mock_encoder(), EncodeVideoFrame(_, _, _))
      .WillOnce([&callback](
                    scoped_refptr<media::VideoFrame> video_frame,
                    base::TimeTicks reference_time,
                    VideoEncoder::FrameEncodedCallback frame_encoded_callback) {
        callback = std::move(frame_encoded_callback);
        return true;
      });

  scoped_refptr<VideoFrame> video_frame = GetNewVideoFrame();
  video_sender().InsertRawVideoFrame(video_frame, NowTicks());

  SetVideoCaptureFeedbackClosure(task_environment().QuitClosure());

  auto encoded_frame = std::make_unique<SenderEncodedFrame>();
  encoded_frame->encoder_utilization = 0.3f;
  encoded_frame->lossiness = 0.5f;
  encoded_frame->encode_completion_time = NowTicks();
  encoded_frame->is_key_frame = true;
  encoded_frame->frame_id = FrameId::first();
  encoded_frame->referenced_frame_id = FrameId::first();
  encoded_frame->rtp_timestamp = RtpTimeTicks(12345);
  encoded_frame->reference_time = NowTicks();
  encoded_frame->data = base::HeapArray<uint8_t>::Uninit(1920 * 1080);

  std::move(callback).Run(std::move(encoded_frame));
  RunUntilQuit();
}

// Make sure we properly handle the frame change callback, even if the encoded
// frame result is nullptr. For more information on this test, see
// https://issuetracker.google.com/393880773.
TEST_P(VideoSenderTest, HandlesNullptrFrameChangeCallback) {
  CreateSender(EncoderType::kMock);

  VideoEncoder::FrameEncodedCallback callback;
  EXPECT_CALL(mock_encoder(), GenerateKeyFrame());
  EXPECT_CALL(mock_encoder(), SetBitRate(5000000));
  EXPECT_CALL(mock_encoder(), EncodeVideoFrame(_, _, _))
      .WillOnce([&callback](
                    scoped_refptr<media::VideoFrame> video_frame,
                    base::TimeTicks reference_time,
                    VideoEncoder::FrameEncodedCallback frame_encoded_callback) {
        callback = std::move(frame_encoded_callback);
        return true;
      });

  scoped_refptr<VideoFrame> video_frame = GetNewVideoFrame();
  video_sender().InsertRawVideoFrame(video_frame, NowTicks());

  const base::TimeDelta backlog_duration =
      static_cast<FrameSender::Client*>(&video_sender())
          ->GetEncoderBacklogDuration();
  std::move(callback).Run(nullptr);

  // The backlog duration should decrease, even if the returned encoded frame
  // was nullptr.
  ASSERT_TRUE(base::test::RunUntil(
      [backlog_duration, video_sender = &video_sender()]() {
        const auto current = static_cast<FrameSender::Client*>(video_sender)
                                 ->GetEncoderBacklogDuration();
        return current < backlog_duration;
      }));
}

TEST_P(VideoSenderTest, ExternalEncoder) {
  CreateSender(EncoderType::kHardware);
  SetVeaFactoryInitializationWillSucceed(true);
  ASSERT_EQ(STATUS_INITIALIZED, status_changes().front());

  // The SizeAdaptableExternalVideoEncoder initially reports STATUS_INITIALIZED
  // so that frames will be sent to it.  Therefore, no encoder activity should
  // have occurred at this point.  Send a frame to spurn creation of the
  // underlying ExternalVideoEncoder instance.
  if (VeaResponseCount() == 0) {
    video_sender().InsertRawVideoFrame(GetNewVideoFrame(), NowTicks());
    RunTasksAndAdvanceClock();
  }
  ASSERT_EQ(STATUS_INITIALIZED, status_changes().front());
  RunTasksAndAdvanceClock(base::Milliseconds(33));

  // VideoSender created an encoder for 1280x720 frames, in order to provide the
  // INITIALIZED status.
  EXPECT_EQ(1, VeaResponseCount());

  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();

  for (int i = 0; i < 3; ++i) {
    video_sender().InsertRawVideoFrame(video_frame, NowTicks());
    RunTasksAndAdvanceClock(base::Milliseconds(33));
    // VideoSender re-created the encoder for the 320x240 frames we're
    // providing.
    EXPECT_EQ(1, VeaResponseCount());
  }

  RunTasksAndAdvanceClock();
  EXPECT_EQ(1, VeaResponseCount());
}

TEST_P(VideoSenderTest, ExternalEncoderInitFails) {
  CreateSender(EncoderType::kHardware);
  SetVeaFactoryInitializationWillSucceed(false);
  EXPECT_EQ(STATUS_INITIALIZED, status_changes().front());

  // Send a frame to spurn creation of the underlying ExternalVideoEncoder
  // instance, which should result in failure.
  video_sender().InsertRawVideoFrame(GetNewVideoFrame(), NowTicks());
  RunTasksAndAdvanceClock();

  EXPECT_THAT(status_changes(), Contains(STATUS_CODEC_INIT_FAILED));
  RunTasksAndAdvanceClock();
}

INSTANTIATE_TEST_SUITE_P(All,
                         VideoSenderTest,
                         ::testing::Bool(),
                         [](const testing::TestParamInfo<bool>& param) {
                           return param.param ? "Experimental" : "Stable";
                         });

}  // namespace media::cast
