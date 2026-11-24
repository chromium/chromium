// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/video_sender.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
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

}  // namespace

class VideoSenderTest : public ::testing::Test, public WithCastEnvironment {
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

  void HandleVideoCaptureFeedback(const VideoCaptureFeedback& feedback) {
    last_feedback_ = feedback;
    if (feedback_closure_) {
      std::move(feedback_closure_).Run();
    }
  }

  const std::optional<VideoCaptureFeedback>& last_feedback() const {
    return last_feedback_;
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
  void CreateSender(
      EncoderType encoder_type,
      std::optional<FrameSenderConfig> custom_config = std::nullopt) {
    FrameSenderConfig video_config =
        custom_config.value_or(GetDefaultVideoSenderConfig());
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
      EXPECT_CALL(*mock_gpu_factories_, IsEncoderSupportKnown())
          .WillRepeatedly(testing::Return(true));
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
        base::BindRepeating(&VideoSenderTest::GetVideoNetworkBandwidth,
                            base::Unretained(this)));

    RunTasksAndAdvanceClock();
  }

  uint32_t GetVideoNetworkBandwidth() const { return current_bandwidth_; }
  void set_bandwidth(uint32_t bandwidth) { current_bandwidth_ = bandwidth; }

  scoped_refptr<VideoFrame> GetNewVideoFrame(
      VideoPixelFormat format = PIXEL_FORMAT_I420) {
    if (first_frame_timestamp_.is_null()) {
      first_frame_timestamp_ = NowTicks();
    }
    constexpr gfx::Size kSize(kWidth, kHeight);
    scoped_refptr<VideoFrame> video_frame =
        VideoFrame::CreateFrame(format, kSize, gfx::Rect(kSize), kSize,
                                NowTicks() - first_frame_timestamp_);
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

  // Destroys the sender, which records end-of-session histograms.
  void DestroySender() {
    mock_encoder_ = nullptr;
    video_sender_.reset();
    RunTasksAndAdvanceClock();
  }

  void EncodeOneFrameAndDestroySender(double encoder_utilization,
                                      std::optional<double> lossiness = 0.1);

  // Results of RunInFlightWindowSimulation().
  struct InFlightWindowSimulationResult {
    int frames_dropped_before_encoding = 0;
    int frames_dropped_after_encoding = 0;
    int key_frame_requests = 0;
  };

  // Streams 300 frames through a VideoSender over a simulated lossless link
  // with a 110 ms round-trip time and a fixed 81.67 ms in-flight window. The
  // encoder takes `encode_latency` per frame. Capture runs at about 23 FPS
  // (43 ms apart) for 100 frames, then at 30 FPS.
  InFlightWindowSimulationResult RunInFlightWindowSimulation(
      base::TimeDelta encode_latency);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> accelerator_task_runner_;

  std::unique_ptr<OpenscreenTestSenders> test_senders_;
  std::vector<OperationalStatus> status_changes_;
  std::unique_ptr<FakeVideoEncodeAcceleratorFactory> vea_factory_;
  int last_pixel_value_;
  base::TimeTicks first_frame_timestamp_;
  base::OnceClosure feedback_closure_;
  std::optional<VideoCaptureFeedback> last_feedback_;
  scoped_refptr<gpu::TestSharedImageInterface> sii_;
  std::unique_ptr<MockGpuVideoAcceleratorFactories> mock_gpu_factories_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<VideoSender> video_sender_;
  raw_ptr<MockVideoEncoder> mock_encoder_ = nullptr;
  uint32_t current_bandwidth_ = openscreen::cast::kDefaultVideoMinBitRate;
};

TEST_F(VideoSenderTest, BuiltInEncoder) {
  CreateSender(EncoderType::kSoftware);
  ASSERT_EQ(STATUS_INITIALIZED, status_changes().front());

  scoped_refptr<VideoFrame> video_frame = GetNewVideoFrame();
  video_sender().InsertRawVideoFrame(video_frame, NowTicks());

  SetVideoCaptureFeedbackClosure(task_environment().QuitClosure());
  RunUntilQuit();
}

TEST_F(VideoSenderTest, MockEncoderGoldenCase) {
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
  encoded_frame->data = base::HeapArray<uint8_t>::WithSize(1920 * 1080);

  std::move(callback).Run(std::move(encoded_frame));
  RunUntilQuit();
}

// When the encoder drops a frame (empty output), the VideoSender should count
// the drop and report full utilization to the capture pipeline, so that the
// capture resolution can be reduced.
TEST_F(VideoSenderTest, EncoderDroppedFrameReportsFeedback) {
  base::HistogramTester histogram_tester;
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

  video_sender().InsertRawVideoFrame(GetNewVideoFrame(), NowTicks());
  SetVideoCaptureFeedbackClosure(task_environment().QuitClosure());

  auto encoded_frame = std::make_unique<SenderEncodedFrame>();
  encoded_frame->encoder_utilization = 0.3;
  encoded_frame->encode_completion_time = NowTicks();
  encoded_frame->frame_id = FrameId::first();
  encoded_frame->referenced_frame_id = FrameId::first();
  encoded_frame->rtp_timestamp = RtpTimeTicks(12345);
  encoded_frame->reference_time = NowTicks();
  // No data: the encoder dropped the frame.

  std::move(callback).Run(std::move(encoded_frame));
  RunUntilQuit();

  ASSERT_TRUE(last_feedback());
  // Full (1.0) utilization, attenuated by the 75% target utilization.
  EXPECT_DOUBLE_EQ(last_feedback()->resource_utilization, 1.0 / 0.75);
  EXPECT_DOUBLE_EQ(video_sender().GetEncoderUtilization(), 0.3);
  // Encoder drops are tracked separately from sender-side drops, which are used
  // to gate bandwidth reductions.
  EXPECT_EQ(video_sender().GetFramesDropped(), 0);
  EXPECT_EQ(video_sender().GetFramesDroppedByEncoder(), 1);
  histogram_tester.ExpectUniqueSample(
      "CastStreaming.Sender.Video.FrameDropped",
      CastStreamingFrameDropReason::kDroppedByEncoder, 1);
}

// Make sure we properly handle the frame change callback, even if the encoded
// frame result is nullptr. For more information on this test, see
// https://issuetracker.google.com/393880773.
TEST_F(VideoSenderTest, HandlesNullptrFrameChangeCallback) {
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

TEST_F(VideoSenderTest, ExternalEncoder) {
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

TEST_F(VideoSenderTest, ExternalEncoderInitFails) {
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

TEST_F(VideoSenderTest, GettersReturnValidValues) {
  CreateSender(EncoderType::kSoftware);
  ASSERT_EQ(STATUS_INITIALIZED, status_changes().front());

  // They should have some default values or zeroes
  EXPECT_GE(video_sender().GetEncoderBitrate(), 0u);
  EXPECT_GE(video_sender().GetEncoderUtilization(), -1.0);  // Defaults to -1.0
  EXPECT_GE(video_sender().GetLossiness(), -1.0);           // Defaults to -1.0
  EXPECT_GE(video_sender().GetFramesInserted(), 0);
  EXPECT_GE(video_sender().GetFramesDropped(), 0);

  // Send a frame to update metrics
  video_sender().InsertRawVideoFrame(GetNewVideoFrame(), NowTicks());
  RunTasksAndAdvanceClock();

  EXPECT_EQ(video_sender().GetFramesInserted(), 1);
  EXPECT_GE(video_sender().GetEncoderBitrate(), 0u);
}

TEST_F(VideoSenderTest, PerformanceOverlayWithNV12) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(media::kCastStreamingPerformanceOverlay);

  CreateSender(EncoderType::kSoftware);
  ASSERT_EQ(STATUS_INITIALIZED, status_changes().front());

  scoped_refptr<VideoFrame> video_frame = GetNewVideoFrame(PIXEL_FORMAT_NV12);
  video_sender().InsertRawVideoFrame(video_frame, NowTicks());

  SetVideoCaptureFeedbackClosure(task_environment().QuitClosure());
  RunUntilQuit();
}

TEST_F(VideoSenderTest, BuiltInEncoderWithNV12) {
  CreateSender(EncoderType::kSoftware);
  ASSERT_EQ(STATUS_INITIALIZED, status_changes().front());

  scoped_refptr<VideoFrame> video_frame = GetNewVideoFrame(PIXEL_FORMAT_NV12);
  video_sender().InsertRawVideoFrame(video_frame, NowTicks());

  SetVideoCaptureFeedbackClosure(task_environment().QuitClosure());
  RunUntilQuit();

  EXPECT_EQ(video_sender().GetFramesInserted(), 1);
}

// Creates a sender with a mock encoder, sends one frame through it that reports
// `encoder_utilization`, and then destroys the sender so that the
// session-level histograms are recorded.
void VideoSenderTest::EncodeOneFrameAndDestroySender(
    double encoder_utilization,
    std::optional<double> lossiness) {
  CreateSender(EncoderType::kMock);

  VideoEncoder::FrameEncodedCallback callback;
  EXPECT_CALL(mock_encoder(), GenerateKeyFrame());
  EXPECT_CALL(mock_encoder(), SetBitRate(_));
  EXPECT_CALL(mock_encoder(), EncodeVideoFrame(_, _, _))
      .WillOnce([&callback](
                    scoped_refptr<media::VideoFrame> video_frame,
                    base::TimeTicks reference_time,
                    VideoEncoder::FrameEncodedCallback frame_encoded_callback) {
        callback = std::move(frame_encoded_callback);
        return true;
      });

  video_sender().InsertRawVideoFrame(GetNewVideoFrame(), NowTicks());
  SetVideoCaptureFeedbackClosure(task_environment().QuitClosure());

  auto encoded_frame = std::make_unique<SenderEncodedFrame>();
  encoded_frame->encoder_utilization = encoder_utilization;
  encoded_frame->lossiness = lossiness;
  encoded_frame->encode_completion_time = NowTicks();
  encoded_frame->is_key_frame = true;
  encoded_frame->frame_id = FrameId::first();
  encoded_frame->referenced_frame_id = FrameId::first();
  encoded_frame->rtp_timestamp = RtpTimeTicks(12345);
  encoded_frame->reference_time = NowTicks();
  encoded_frame->data = base::HeapArray<uint8_t>::WithSize(1024);
  std::move(callback).Run(std::move(encoded_frame));
  RunUntilQuit();

  DestroySender();
}

TEST_F(VideoSenderTest, RecordsAverageEncoderUtilization) {
  base::HistogramTester histogram_tester;
  EncodeOneFrameAndDestroySender(0.3);

  // The mock encoder path is configured as a software encoder.
  histogram_tester.ExpectUniqueSample(
      "CastStreaming.Sender.Video.AverageEncoderUtilization.Software", 30, 1);
  histogram_tester.ExpectUniqueSample(
      "CastStreaming.Sender.Video.PercentFramesEncoderOverutilized.Software", 0,
      1);
  histogram_tester.ExpectTotalCount(
      "CastStreaming.Sender.Video.AverageEncoderUtilization.Hardware", 0);
  histogram_tester.ExpectTotalCount(
      "CastStreaming.Sender.Video.PercentFramesEncoderOverutilized.Hardware",
      0);
}

TEST_F(VideoSenderTest, RecordsOverutilizedEncoderFrames) {
  base::HistogramTester histogram_tester;
  EncodeOneFrameAndDestroySender(1.5);

  histogram_tester.ExpectUniqueSample(
      "CastStreaming.Sender.Video.AverageEncoderUtilization.Software", 150, 1);
  histogram_tester.ExpectUniqueSample(
      "CastStreaming.Sender.Video.PercentFramesEncoderOverutilized.Software",
      100, 1);
}

TEST_F(VideoSenderTest, DoesNotRecordEncoderUtilizationWithoutFrames) {
  base::HistogramTester histogram_tester;
  CreateSender(EncoderType::kSoftware);
  DestroySender();

  histogram_tester.ExpectTotalCount(
      "CastStreaming.Sender.Video.AverageEncoderUtilization.Software", 0);
  histogram_tester.ExpectTotalCount(
      "CastStreaming.Sender.Video.AverageEncoderUtilization.Hardware", 0);
  histogram_tester.ExpectTotalCount(
      "CastStreaming.Sender.Video.PercentFramesEncoderOverutilized.Software",
      0);
  histogram_tester.ExpectTotalCount(
      "CastStreaming.Sender.Video.PercentFramesEncoderOverutilized.Hardware",
      0);
  histogram_tester.ExpectTotalCount(
      "CastStreaming.Sender.Video.AverageLossiness", 0);
  histogram_tester.ExpectTotalCount(
      "CastStreaming.Sender.Video.AverageTargetBitrate", 0);
  histogram_tester.ExpectTotalCount(
      "CastStreaming.Sender.Video.PercentTimeAtMaxBitrate", 0);
}

TEST_F(VideoSenderTest, RecordsAverageLossiness) {
  base::HistogramTester histogram_tester;
  EncodeOneFrameAndDestroySender(0.3, /*lossiness=*/0.1);

  histogram_tester.ExpectUniqueSample(
      "CastStreaming.Sender.Video.AverageLossiness", 10, 1);
}

TEST_F(VideoSenderTest, DoesNotRecordAverageLossinessWhenNotComputed) {
  base::HistogramTester histogram_tester;
  EncodeOneFrameAndDestroySender(0.3, /*lossiness=*/std::nullopt);

  histogram_tester.ExpectTotalCount(
      "CastStreaming.Sender.Video.AverageLossiness", 0);
}

TEST_F(VideoSenderTest, RecordsTimeWeightedBitrateMetrics) {
  base::HistogramTester histogram_tester;
  FrameSenderConfig config = GetDefaultVideoSenderConfig();
  config.min_bitrate = 1000000;
  config.max_bitrate = 4000000;
  set_bandwidth(4000000);
  CreateSender(EncoderType::kMock, config);

  EXPECT_CALL(mock_encoder(), GenerateKeyFrame()).Times(AtLeast(1));
  EXPECT_CALL(mock_encoder(), SetBitRate(4000000));
  EXPECT_CALL(mock_encoder(), SetBitRate(2000000));
  EXPECT_CALL(mock_encoder(), EncodeVideoFrame(_, _, _))
      .WillRepeatedly(testing::Return(true));

  // 3 seconds at the maximum of 4 Mbps...
  video_sender().InsertRawVideoFrame(GetNewVideoFrame(), NowTicks());
  AdvanceClock(base::Seconds(3));

  // ...then 1 second at 2 Mbps.
  set_bandwidth(2000000);
  video_sender().InsertRawVideoFrame(GetNewVideoFrame(), NowTicks());
  AdvanceClock(base::Seconds(1));
  DestroySender();

  // (3 s * 4000 kbps + 1 s * 2000 kbps) / 4 s = 3500 kbps.
  histogram_tester.ExpectUniqueSample(
      "CastStreaming.Sender.Video.AverageTargetBitrate", 3500, 1);
  histogram_tester.ExpectUniqueSample(
      "CastStreaming.Sender.Video.PercentTimeAtMaxBitrate", 75, 1);
}

TEST_F(VideoSenderTest,
       DoesNotDropFrameAfterStaticScreenGapWithFrameInEncoder) {
  CreateSender(EncoderType::kMock);

  VideoEncoder::FrameEncodedCallback first_callback;
  EXPECT_CALL(mock_encoder(), GenerateKeyFrame());
  EXPECT_CALL(mock_encoder(), SetBitRate(5000000));
  EXPECT_CALL(mock_encoder(), EncodeVideoFrame(_, _, _))
      .WillOnce([&first_callback](
                    scoped_refptr<media::VideoFrame> video_frame,
                    base::TimeTicks reference_time,
                    VideoEncoder::FrameEncodedCallback frame_encoded_callback) {
        first_callback = std::move(frame_encoded_callback);
        return true;
      })
      .WillOnce([](scoped_refptr<media::VideoFrame> video_frame,
                   base::TimeTicks reference_time,
                   VideoEncoder::FrameEncodedCallback frame_encoded_callback) {
        return true;
      });

  // Insert first frame into the encoder; keep it in the encoder while a static
  // screen gap elapses.
  video_sender().InsertRawVideoFrame(GetNewVideoFrame(), NowTicks());
  EXPECT_EQ(video_sender().GetFramesInserted(), 1);
  EXPECT_EQ(video_sender().GetFramesDropped(), 0);

  // Advance clock by 2 seconds (simulating a static screen gap exceeding the
  // 1-second max in-flight duration). The next frame must not treat the 2s idle
  // capture gap as encoder backlog duration and must not be dropped.
  AdvanceClock(base::Seconds(2));
  video_sender().InsertRawVideoFrame(GetNewVideoFrame(), NowTicks());
  EXPECT_EQ(video_sender().GetFramesInserted(), 2);
  EXPECT_EQ(video_sender().GetFramesDropped(), 0);

  // When the first frame finishes encoding, GetEncoderUtilization() and
  // GetLossiness() reflect the encoded frame's values immediately.
  auto encoded_frame = std::make_unique<SenderEncodedFrame>();
  encoded_frame->encoder_utilization = 0.42;
  encoded_frame->lossiness = 0.25;
  encoded_frame->encode_completion_time = NowTicks();
  encoded_frame->is_key_frame = true;
  encoded_frame->frame_id = FrameId::first();
  encoded_frame->referenced_frame_id = FrameId::first();
  encoded_frame->rtp_timestamp = RtpTimeTicks(90);
  encoded_frame->reference_time = NowTicks();
  encoded_frame->data = base::HeapArray<uint8_t>::WithSize(512);
  std::move(first_callback).Run(std::move(encoded_frame));
  EXPECT_DOUBLE_EQ(video_sender().GetEncoderUtilization(), 0.42);
  EXPECT_DOUBLE_EQ(video_sender().GetLossiness(), 0.25);
}

TEST_F(VideoSenderTest, EnqueueFrameDropCountsAsDroppedFrame) {
  base::HistogramTester histogram_tester;
  CreateSender(EncoderType::kMock);

  VideoEncoder::FrameEncodedCallback callback;
  EXPECT_CALL(mock_encoder(), GenerateKeyFrame()).Times(2);
  EXPECT_CALL(mock_encoder(), SetBitRate(5000000));
  EXPECT_CALL(mock_encoder(), EncodeVideoFrame(_, _, _))
      .WillOnce([&callback](
                    scoped_refptr<media::VideoFrame> video_frame,
                    base::TimeTicks reference_time,
                    VideoEncoder::FrameEncodedCallback frame_encoded_callback) {
        callback = std::move(frame_encoded_callback);
        return true;
      });

  video_sender().InsertRawVideoFrame(GetNewVideoFrame(), NowTicks());
  SetVideoCaptureFeedbackClosure(task_environment().QuitClosure());

  auto encoded_frame = std::make_unique<SenderEncodedFrame>();
  encoded_frame->encoder_utilization = 0.3;
  encoded_frame->lossiness = 0.1f;
  encoded_frame->encode_completion_time = NowTicks();
  encoded_frame->is_key_frame = false;
  encoded_frame->frame_id = FrameId::first();
  encoded_frame->referenced_frame_id = FrameId::first() - 1;
  encoded_frame->rtp_timestamp = RtpTimeTicks(12345);
  encoded_frame->reference_time = NowTicks();
  encoded_frame->data = base::HeapArray<uint8_t>::WithSize(1024);

  std::move(callback).Run(std::move(encoded_frame));
  RunUntilQuit();

  EXPECT_EQ(video_sender().GetFramesInserted(), 1);
  EXPECT_EQ(video_sender().GetFramesDropped(), 1);
  EXPECT_EQ(video_sender().GetFramesDroppedBeforeEncoding(), 0);
  EXPECT_EQ(video_sender().GetFramesDroppedAfterEncoding(), 1);
  histogram_tester.ExpectUniqueSample(
      "CastStreaming.Sender.Video.FrameDropped",
      CastStreamingFrameDropReason::kInvalidReferencedFrameId, 1);
}

TEST_F(VideoSenderTest, ClampsBitrateToConfiguredBounds) {
  FrameSenderConfig config = GetDefaultVideoSenderConfig();
  config.min_bitrate = 500000;
  config.max_bitrate = 5000000;
  set_bandwidth(100000);  // Below min_bitrate.

  CreateSender(EncoderType::kMock, config);

  EXPECT_CALL(mock_encoder(), GenerateKeyFrame()).Times(AtLeast(1));
  // Below min_bitrate -> clamped to min_bitrate (500000).
  EXPECT_CALL(mock_encoder(), SetBitRate(500000));
  EXPECT_CALL(mock_encoder(), EncodeVideoFrame(_, _, _))
      .WillRepeatedly(testing::Return(true));

  video_sender().InsertRawVideoFrame(GetNewVideoFrame(), NowTicks());
  RunTasksAndAdvanceClock(base::Milliseconds(33));

  // Within range, changed by more than 5% -> uses bandwidth (2000000).
  set_bandwidth(2000000);
  EXPECT_CALL(mock_encoder(), SetBitRate(2000000));
  video_sender().InsertRawVideoFrame(GetNewVideoFrame(), NowTicks());
  RunTasksAndAdvanceClock(base::Milliseconds(33));

  // Small change (<= 5%) -> hysteresis prevents update.
  set_bandwidth(2050000);
  video_sender().InsertRawVideoFrame(GetNewVideoFrame(), NowTicks());
  RunTasksAndAdvanceClock(base::Milliseconds(33));

  // Above max_bitrate -> clamped to max_bitrate (5000000).
  set_bandwidth(10000000);
  EXPECT_CALL(mock_encoder(), SetBitRate(5000000));
  video_sender().InsertRawVideoFrame(GetNewVideoFrame(), NowTicks());
  RunTasksAndAdvanceClock(base::Milliseconds(33));
}

VideoSenderTest::InFlightWindowSimulationResult
VideoSenderTest::RunInFlightWindowSimulation(base::TimeDelta encode_latency) {
  FrameSenderConfig video_config = GetDefaultVideoSenderConfig();
  video_config.min_playout_delay = base::Milliseconds(200);
  video_config.max_playout_delay = base::Milliseconds(200);

  auto test_senders =
      std::make_unique<OpenscreenTestSenders>(OpenscreenTestSenders::Config(
          GetMainThreadTaskRunner(), GetMockTickClock(), std::nullopt,
          openscreen::cast::RtpPayloadType::kVideoVp8, std::nullopt,
          video_config));
  MockSender* mock_sender = test_senders->video_sender.get();

  constexpr auto kMaxInFlight = std::chrono::microseconds(81667);
  constexpr base::TimeDelta kRoundTripTime = base::Milliseconds(110);
  // Matches kMaxUnpausedFrameDuration in Open Screen's sender_impl.cc.
  constexpr auto kMaxStepDuration = std::chrono::milliseconds(50);

  // Tracks the frames the simulated Sender has accepted, and ACKs each one a
  // round trip after it is enqueued. With a constant round-trip time, frames
  // are ACKed in order.
  struct SimulatedSender {
    explicit SimulatedSender(base::WeakPtr<MockSender> sender)
        : sender(std::move(sender)) {}

    void AckFrame(openscreen::cast::FrameId frame_id) {
      unacked_frames.erase(frame_id);
      if (sender && sender->observer()) {
        sender->observer()->OnFrameCanceled(frame_id);
      }
    }

    base::WeakPtr<MockSender> sender;
    openscreen::cast::FrameId next_frame_id =
        openscreen::cast::FrameId::first();
    bool has_enqueued_key_frame = false;
    std::map<openscreen::cast::FrameId, openscreen::cast::RtpTimeTicks>
        unacked_frames;
    base::WeakPtrFactory<SimulatedSender> weak_factory{this};
  } simulated_sender(mock_sender->AsWeakPtr());

  EXPECT_CALL(*mock_sender, SetObserver(_))
      .WillRepeatedly([mock_sender](openscreen::cast::Sender::Observer* o) {
        mock_sender->set_observer(o);
      });
  EXPECT_CALL(*mock_sender, GetMaxInFlightMediaDuration)
      .WillRepeatedly(testing::Return(kMaxInFlight));
  EXPECT_CALL(*mock_sender, GetCurrentRoundTripTime)
      .WillRepeatedly(testing::Return(std::chrono::milliseconds(110)));
  EXPECT_CALL(*mock_sender, NeedsKeyFrame).WillRepeatedly([&]() {
    return !simulated_sender.has_enqueued_key_frame;
  });
  EXPECT_CALL(*mock_sender, GetNextFrameId).WillRepeatedly([&]() {
    return simulated_sender.next_frame_id;
  });
  EXPECT_CALL(*mock_sender, GetInFlightFrameCount).WillRepeatedly([&]() {
    return simulated_sender.unacked_frames.size();
  });
  // Mirrors SenderImpl::GetInFlightMediaDuration(): sums the steps between
  // consecutive unACKed frames and from the newest one to `next_rtp`, capping
  // each step at kMaxStepDuration.
  EXPECT_CALL(*mock_sender, GetInFlightMediaDuration)
      .WillRepeatedly([&](openscreen::cast::RtpTimeTicks next_rtp)
                          -> openscreen::Clock::duration {
        if (simulated_sender.unacked_frames.empty()) {
          return openscreen::Clock::duration::zero();
        }
        const auto capped_step = [&](openscreen::cast::RtpTimeTicks from,
                                     openscreen::cast::RtpTimeTicks to) {
          return std::min<openscreen::Clock::duration>(
              (to - from).ToDuration<openscreen::Clock::duration>(
                  video_config.rtp_timebase),
              kMaxStepDuration);
        };
        openscreen::Clock::duration total{};
        openscreen::cast::RtpTimeTicks previous =
            simulated_sender.unacked_frames.begin()->second;
        for (const auto& [frame_id, rtp_timestamp] :
             simulated_sender.unacked_frames) {
          total += capped_step(previous, rtp_timestamp);
          previous = rtp_timestamp;
        }
        return total + capped_step(previous, next_rtp);
      });
  // Mirrors the MAX_DURATION_IN_FLIGHT check in SenderImpl::EnqueueFrame().
  EXPECT_CALL(*mock_sender, EnqueueFrame)
      .WillRepeatedly([&](const openscreen::cast::EncodedFrame& frame) {
        if (mock_sender->GetInFlightMediaDuration(frame.rtp_timestamp) >
            kMaxInFlight) {
          return openscreen::cast::Sender::EnqueueFrameResult::
              MAX_DURATION_IN_FLIGHT;
        }
        simulated_sender.unacked_frames[frame.frame_id] = frame.rtp_timestamp;
        simulated_sender.next_frame_id = frame.frame_id + 1;
        if (frame.dependency ==
            openscreen::cast::EncodedFrame::Dependency::kKeyFrame) {
          simulated_sender.has_enqueued_key_frame = true;
        }
        GetMainThreadTaskRunner()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&SimulatedSender::AckFrame,
                           simulated_sender.weak_factory.GetWeakPtr(),
                           frame.frame_id),
            kRoundTripTime);
        return openscreen::cast::Sender::EnqueueFrameResult::OK;
      });

  // Simulates an encoder that finishes each frame `encode_latency` after it
  // receives it, so frames can queue up in the encoder.
  auto mock_encoder = std::make_unique<MockVideoEncoder>();
  InFlightWindowSimulationResult result;
  bool force_next_key_frame = true;
  FrameId next_encoder_frame_id = FrameId::first();
  EXPECT_CALL(*mock_encoder, GenerateKeyFrame()).WillRepeatedly([&]() {
    ++result.key_frame_requests;
    force_next_key_frame = true;
  });
  EXPECT_CALL(*mock_encoder, SetBitRate(_)).Times(AtLeast(1));
  EXPECT_CALL(*mock_encoder, EncodeVideoFrame(_, _, _))
      .WillRepeatedly(
          [&](scoped_refptr<media::VideoFrame> video_frame,
              base::TimeTicks reference_time,
              VideoEncoder::FrameEncodedCallback frame_encoded_callback) {
            auto encoded = std::make_unique<SenderEncodedFrame>();
            encoded->encoder_utilization = 0.15;
            encoded->lossiness = 0.05;
            encoded->encode_completion_time = reference_time + encode_latency;
            encoded->is_key_frame = force_next_key_frame;
            encoded->frame_id = next_encoder_frame_id++;
            encoded->referenced_frame_id = encoded->is_key_frame
                                               ? encoded->frame_id
                                               : encoded->frame_id - 1;
            encoded->rtp_timestamp =
                ToRtpTimeTicks(video_frame->timestamp(), kVideoFrequency);
            encoded->reference_time = reference_time;
            encoded->data = base::HeapArray<uint8_t>::WithSize(64);
            force_next_key_frame = false;
            GetMainThreadTaskRunner()->PostDelayedTask(
                FROM_HERE,
                base::BindOnce(std::move(frame_encoded_callback),
                               std::move(encoded)),
                encode_latency);
            return true;
          });

  VideoSender sender(std::move(mock_encoder), cast_environment(), video_config,
                     std::move(test_senders->video_sender),
                     base::BindRepeating(&IgnorePlayoutDelayChanges),
                     base::DoNothing(),
                     base::BindRepeating([]() -> uint32_t { return 5000000; }));

  scoped_refptr<VideoFrame> black_frame =
      VideoFrame::CreateBlackFrame(gfx::Size(kWidth, kHeight));
  const base::TimeTicks start_time = NowTicks();
  for (int i = 0; i < 300; ++i) {
    scoped_refptr<VideoFrame> frame = VideoFrame::WrapVideoFrame(
        black_frame, black_frame->format(), black_frame->visible_rect(),
        black_frame->natural_size());
    frame->set_timestamp(NowTicks() - start_time);
    sender.InsertRawVideoFrame(std::move(frame), NowTicks());
    task_environment().FastForwardBy(i < 100 ? base::Milliseconds(43)
                                             : base::Milliseconds(33));
  }
  // Let outstanding encodes and ACKs finish.
  task_environment().FastForwardBy(encode_latency + kRoundTripTime);

  result.frames_dropped_before_encoding =
      sender.GetFramesDroppedBeforeEncoding();
  result.frames_dropped_after_encoding = sender.GetFramesDroppedAfterEncoding();
  return result;
}

// Frames that would exceed the Open Screen Sender's in-flight window must be
// dropped before encoding, not rejected after it. A frame rejected after
// encoding has already updated the encoder's reference buffer, so the next
// frame can't be decoded and VideoSender has to request a key frame. See
// b/555393284.
//
// With a 4 ms encoder, the encoder is empty whenever a new frame arrives. At
// 23 FPS, the third unACKed frame is 86 ms after the first, which used to pass
// the estimate in ShouldDropNextFrame() (43 + 33.3 = 76.3 ms) and then fail
// Sender::EnqueueFrame().
TEST_F(VideoSenderTest, DropsFramesBeforeEncodingWhenInFlightWindowIsFull) {
  base::HistogramTester histogram_tester;
  const InFlightWindowSimulationResult result =
      RunInFlightWindowSimulation(base::Milliseconds(4));

  // With a 110 ms round trip, some frames must still be dropped to fit the
  // in-flight window, but only before encoding.
  EXPECT_GT(result.frames_dropped_before_encoding, 0);
  EXPECT_EQ(result.frames_dropped_after_encoding, 0);
  EXPECT_EQ(
      histogram_tester.GetBucketCount(
          "CastStreaming.Sender.Video.FrameDropped",
          CastStreamingFrameDropReason::kInFlightDurationTooHighAfterEncoding),
      0);
  // Only the initial key frame is requested.
  EXPECT_EQ(result.key_frame_requests, 1);
}

// Same as above, but the encoder takes 120 ms, so two or more frames are still
// in the encoder when the next one arrives. The Sender doesn't know about those
// frames yet and caps the step from its newest frame at 50 ms, so it would
// undercount them. ShouldDropNextFrame() must keep counting the encoder backlog
// instead.
TEST_F(VideoSenderTest,
       DropsFramesBeforeEncodingWhenInFlightWindowIsFullWithEncoderBacklog) {
  base::HistogramTester histogram_tester;
  const InFlightWindowSimulationResult result =
      RunInFlightWindowSimulation(base::Milliseconds(120));

  EXPECT_GT(result.frames_dropped_before_encoding, 0);
  EXPECT_EQ(result.frames_dropped_after_encoding, 0);
  EXPECT_EQ(
      histogram_tester.GetBucketCount(
          "CastStreaming.Sender.Video.FrameDropped",
          CastStreamingFrameDropReason::kInFlightDurationTooHighAfterEncoding),
      0);
  EXPECT_EQ(result.key_frame_requests, 1);
}

TEST_F(VideoSenderTest, SetMaxFrameRateUpdatesFrameSenderAndEncoder) {
  FrameSenderConfig config = GetDefaultVideoSenderConfig();
  config.max_frame_rate = 60.0;
  CreateSender(EncoderType::kMock, config);

  EXPECT_DOUBLE_EQ(video_sender().GetMaxFrameRate(), 60.0);
  EXPECT_CALL(mock_encoder(), SetMaxFrameRate(30.0));
  video_sender().SetMaxFrameRate(30.0);
  EXPECT_DOUBLE_EQ(video_sender().GetMaxFrameRate(), 30.0);
}

}  // namespace media::cast
