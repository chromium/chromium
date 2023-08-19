// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/power_monitor/power_monitor.h"
#include "base/run_loop.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/power_monitor_test.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_suite.h"
#include "base/time/time.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/cast/common/openscreen_conversion_helpers.h"
#include "media/cast/common/rtp_time.h"
#include "media/cast/common/sender_encoded_frame.h"
#include "media/cast/common/video_frame_factory.h"
#include "media/cast/constants.h"
#include "media/cast/encoding/h264_vt_encoder.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/video_utility.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_glue.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

using Dependency = openscreen::cast::EncodedFrame::Dependency;

namespace {

const int kVideoWidth = 1280;
const int kVideoHeight = 720;

class MediaTestSuite final : public base::TestSuite {
 public:
  MediaTestSuite(int argc, char** argv) : TestSuite(argc, argv) {}
  ~MediaTestSuite() override = default;

 protected:
  void Initialize() final;
};

void MediaTestSuite::Initialize() {
  base::TestSuite::Initialize();
  media::InitializeMediaLibrary();
}

}  // namespace

namespace media {
namespace cast {

// See comment in end2end_unittest.cc for details on this value.
const double kVideoAcceptedPSNR = 38.0;

void SaveDecoderInitResult(bool* out_result, DecoderStatus in_result) {
  *out_result = in_result.is_ok();
}

void SaveOperationalStatus(OperationalStatus* out_status,
                           OperationalStatus in_status) {
  *out_status = in_status;
}

class MetadataRecorder : public base::RefCountedThreadSafe<MetadataRecorder> {
 public:
  MetadataRecorder() : count_frames_delivered_(0) {}

  MetadataRecorder(const MetadataRecorder&) = delete;
  MetadataRecorder& operator=(const MetadataRecorder&) = delete;

  int count_frames_delivered() const { return count_frames_delivered_; }

  void PushExpectation(FrameId expected_frame_id,
                       FrameId expected_last_referenced_frame_id,
                       RtpTimeTicks expected_rtp_timestamp,
                       base::TimeTicks expected_reference_time) {
    expectations_.push(
        Expectation{expected_frame_id, expected_last_referenced_frame_id,
                    expected_rtp_timestamp, expected_reference_time});
  }

  void CompareFrameWithExpected(
      std::unique_ptr<SenderEncodedFrame> encoded_frame) {
    ASSERT_LT(0u, expectations_.size());
    auto e = expectations_.front();
    expectations_.pop();
    if (e.expected_frame_id != e.expected_last_referenced_frame_id) {
      EXPECT_EQ(Dependency::kDependent, encoded_frame->dependency);
    } else {
      EXPECT_EQ(Dependency::kKeyFrame, encoded_frame->dependency);
    }
    EXPECT_EQ(e.expected_frame_id, encoded_frame->frame_id);
    EXPECT_EQ(e.expected_last_referenced_frame_id,
              encoded_frame->referenced_frame_id)
        << "frame id: " << e.expected_frame_id;
    EXPECT_EQ(e.expected_rtp_timestamp, encoded_frame->rtp_timestamp);
    EXPECT_EQ(e.expected_reference_time, encoded_frame->reference_time);
    EXPECT_FALSE(encoded_frame->data.empty());
    ++count_frames_delivered_;
  }

 private:
  friend class base::RefCountedThreadSafe<MetadataRecorder>;
  virtual ~MetadataRecorder() {}

  int count_frames_delivered_;

  struct Expectation {
    FrameId expected_frame_id;
    FrameId expected_last_referenced_frame_id;
    RtpTimeTicks expected_rtp_timestamp;
    base::TimeTicks expected_reference_time;
  };
  base::queue<Expectation> expectations_;
};

class EndToEndFrameChecker
    : public base::RefCountedThreadSafe<EndToEndFrameChecker> {
 public:
  explicit EndToEndFrameChecker(const VideoDecoderConfig& config)
      : decoder_(&media_log_), count_frames_checked_(0) {
    bool decoder_init_result;
    decoder_.Initialize(
        config, false, nullptr,
        base::BindOnce(&SaveDecoderInitResult, &decoder_init_result),
        base::BindRepeating(&EndToEndFrameChecker::CompareFrameWithExpected,
                            base::Unretained(this)),
        base::NullCallback());
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(decoder_init_result);
  }

  EndToEndFrameChecker(const EndToEndFrameChecker&) = delete;
  EndToEndFrameChecker& operator=(const EndToEndFrameChecker&) = delete;

  void PushExpectation(scoped_refptr<VideoFrame> frame) {
    expectations_.push(std::move(frame));
  }

  void EncodeDone(std::unique_ptr<SenderEncodedFrame> encoded_frame) {
    auto buffer = DecoderBuffer::CopyFrom(encoded_frame->bytes(),
                                          encoded_frame->data.size());
    decoder_.Decode(buffer, base::BindOnce(&EndToEndFrameChecker::DecodeDone,
                                           base::Unretained(this)));
  }

  void CompareFrameWithExpected(scoped_refptr<VideoFrame> frame) {
    ASSERT_LT(0u, expectations_.size());
    auto& e = expectations_.front();
    expectations_.pop();
    EXPECT_LE(kVideoAcceptedPSNR, I420PSNR(*e, *frame));
    ++count_frames_checked_;
  }

  void DecodeDone(DecoderStatus status) { EXPECT_TRUE(status.is_ok()); }

  int count_frames_checked() const { return count_frames_checked_; }

 private:
  friend class base::RefCountedThreadSafe<EndToEndFrameChecker>;
  virtual ~EndToEndFrameChecker() {}

  NullMediaLog media_log_;
  FFmpegVideoDecoder decoder_;
  base::queue<scoped_refptr<VideoFrame>> expectations_;
  int count_frames_checked_;
};

void CreateFrameAndMemsetPlane(VideoFrameFactory* const video_frame_factory) {
  const scoped_refptr<media::VideoFrame> video_frame =
      video_frame_factory->MaybeCreateFrame(
          gfx::Size(kVideoWidth, kVideoHeight), base::TimeDelta());
  ASSERT_TRUE(video_frame.get());
  auto* cv_pixel_buffer = video_frame->CvPixelBuffer();
  ASSERT_TRUE(cv_pixel_buffer);
  CVPixelBufferLockBaseAddress(cv_pixel_buffer, 0);
  auto* ptr = CVPixelBufferGetBaseAddressOfPlane(cv_pixel_buffer, 0);
  ASSERT_TRUE(ptr);
  memset(ptr, 0xfe,
         CVPixelBufferGetBytesPerRowOfPlane(cv_pixel_buffer, 0) *
             CVPixelBufferGetHeightOfPlane(cv_pixel_buffer, 0));
  CVPixelBufferUnlockBaseAddress(cv_pixel_buffer, 0);
}

class TestPowerSource : public base::PowerMonitorSource {
 public:
  void GenerateSuspendEvent() {
    ProcessPowerEvent(SUSPEND_EVENT);
    base::RunLoop().RunUntilIdle();
  }
  void GenerateResumeEvent() {
    ProcessPowerEvent(RESUME_EVENT);
    base::RunLoop().RunUntilIdle();
  }

  // base::PowerMonitorSource override:
  bool IsOnBatteryPower() final { return false; }
};

class H264VideoToolboxEncoderTest : public ::testing::Test {
 public:
  H264VideoToolboxEncoderTest(const H264VideoToolboxEncoderTest&) = delete;
  H264VideoToolboxEncoderTest& operator=(const H264VideoToolboxEncoderTest&) =
      delete;

 protected:
  H264VideoToolboxEncoderTest() = default;

  void SetUp() final {
    clock_.Advance(base::TimeTicks::Now() - base::TimeTicks());

    power_source_ = new TestPowerSource();
    base::PowerMonitor::Initialize(
        std::unique_ptr<TestPowerSource>(power_source_));

    cast_environment_ = new CastEnvironment(
        &clock_, task_environment_.GetMainThreadTaskRunner(),
        task_environment_.GetMainThreadTaskRunner(),
        task_environment_.GetMainThreadTaskRunner());
    encoder_ = std::make_unique<H264VideoToolboxEncoder>(
        cast_environment_, video_sender_config_,
        std::make_unique<MockVideoEncoderMetricsProvider>(),
        base::BindRepeating(&SaveOperationalStatus, &operational_status_));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(STATUS_INITIALIZED, operational_status_);
  }

  void TearDown() final {
    encoder_.reset();
    base::RunLoop().RunUntilIdle();
    base::PowerMonitor::ShutdownForTesting();
  }

  void AdvanceClockAndVideoFrameTimestamp() {
    clock_.Advance(base::Milliseconds(33));
    frame_->set_timestamp(frame_->timestamp() + base::Milliseconds(33));
  }

  static void SetUpTestCase() {
    // Reusable test data.
    video_sender_config_ = GetDefaultVideoSenderConfig();
    video_sender_config_.codec = Codec::kVideoH264;
    const gfx::Size size(kVideoWidth, kVideoHeight);
    frame_ = media::VideoFrame::CreateFrame(
        PIXEL_FORMAT_I420, size, gfx::Rect(size), size, base::TimeDelta());
    PopulateVideoFrame(frame_.get(), 123);
  }

  static void TearDownTestCase() { frame_ = nullptr; }

  static scoped_refptr<media::VideoFrame> frame_;
  static FrameSenderConfig video_sender_config_;

  base::SimpleTestTickClock clock_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<CastEnvironment> cast_environment_;
  std::unique_ptr<VideoEncoder> encoder_;
  OperationalStatus operational_status_;
  raw_ptr<TestPowerSource, DanglingUntriaged>
      power_source_;  // Owned by the power monitor.
};

// static
scoped_refptr<media::VideoFrame> H264VideoToolboxEncoderTest::frame_;
FrameSenderConfig H264VideoToolboxEncoderTest::video_sender_config_;

// Failed on mac-rel trybot. http://crbug.com/627260
TEST_F(H264VideoToolboxEncoderTest, DISABLED_CheckFrameMetadataSequence) {
  auto metadata_recorder = base::MakeRefCounted<MetadataRecorder>();
  metadata_recorder->PushExpectation(
      FrameId::first(), FrameId::first(),
      ToRtpTimeTicks(frame_->timestamp(), kVideoFrequency), clock_.NowTicks());
  EXPECT_TRUE(encoder_->EncodeVideoFrame(
      frame_, clock_.NowTicks(),
      base::BindOnce(&MetadataRecorder::CompareFrameWithExpected,
                     metadata_recorder.get())));
  base::RunLoop().RunUntilIdle();

  for (FrameId frame_id = FrameId::first() + 1;
       frame_id < FrameId::first() + 10; ++frame_id) {
    AdvanceClockAndVideoFrameTimestamp();
    metadata_recorder->PushExpectation(
        frame_id, frame_id - 1,
        ToRtpTimeTicks(frame_->timestamp(), kVideoFrequency),
        clock_.NowTicks());
    EXPECT_TRUE(encoder_->EncodeVideoFrame(
        frame_, clock_.NowTicks(),
        base::BindOnce(&MetadataRecorder::CompareFrameWithExpected,
                       metadata_recorder.get())));
  }

  encoder_.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(10, metadata_recorder->count_frames_delivered());
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
// Failed on mac-rel trybot. http://crbug.com/627260
TEST_F(H264VideoToolboxEncoderTest, DISABLED_CheckFramesAreDecodable) {
  const auto alpha_mode = IsOpaque(frame_->format())
                              ? VideoDecoderConfig::AlphaMode::kIsOpaque
                              : VideoDecoderConfig::AlphaMode::kHasAlpha;
  VideoDecoderConfig config(
      VideoCodec::kH264, H264PROFILE_MAIN, alpha_mode, VideoColorSpace(),
      kNoTransformation, frame_->coded_size(), frame_->visible_rect(),
      frame_->natural_size(), EmptyExtraData(), EncryptionScheme::kUnencrypted);
  scoped_refptr<EndToEndFrameChecker> checker(new EndToEndFrameChecker(config));

  for (FrameId frame_id = FrameId::first(); frame_id < FrameId::first() + 6;
       ++frame_id) {
    checker->PushExpectation(frame_);
    EXPECT_TRUE(encoder_->EncodeVideoFrame(
        frame_, clock_.NowTicks(),
        base::BindOnce(&EndToEndFrameChecker::EncodeDone, checker.get())));
    AdvanceClockAndVideoFrameTimestamp();
  }

  encoder_.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(5, checker->count_frames_checked());
}
#endif

TEST_F(H264VideoToolboxEncoderTest, CheckVideoFrameFactory) {
  auto video_frame_factory = encoder_->CreateVideoFrameFactory();
  ASSERT_TRUE(video_frame_factory.get());
  // The first call to |MaybeCreateFrame| will return null but post a task to
  // the encoder to initialize for the specified frame size. We then drain the
  // message loop. After that, the encoder should have initialized and we
  // request a frame again.
  ASSERT_FALSE(video_frame_factory->MaybeCreateFrame(
      gfx::Size(kVideoWidth, kVideoHeight), base::TimeDelta()));
  base::RunLoop().RunUntilIdle();
  CreateFrameAndMemsetPlane(video_frame_factory.get());
}

TEST_F(H264VideoToolboxEncoderTest, CheckPowerMonitoring) {
  // Encode a frame, suspend, encode a frame, resume, encode a frame.

  EXPECT_TRUE(
      encoder_->EncodeVideoFrame(frame_, clock_.NowTicks(), base::DoNothing()));
  power_source_->GenerateSuspendEvent();
  EXPECT_FALSE(
      encoder_->EncodeVideoFrame(frame_, clock_.NowTicks(), base::DoNothing()));
  power_source_->GenerateResumeEvent();
  EXPECT_TRUE(
      encoder_->EncodeVideoFrame(frame_, clock_.NowTicks(), base::DoNothing()));
}

TEST_F(H264VideoToolboxEncoderTest, CheckPowerMonitoringNoInitialFrame) {
  // Suspend, encode a frame, resume, encode a frame.

  power_source_->GenerateSuspendEvent();
  EXPECT_FALSE(
      encoder_->EncodeVideoFrame(frame_, clock_.NowTicks(), base::DoNothing()));
  power_source_->GenerateResumeEvent();
  EXPECT_TRUE(
      encoder_->EncodeVideoFrame(frame_, clock_.NowTicks(), base::DoNothing()));
}

TEST_F(H264VideoToolboxEncoderTest, CheckPowerMonitoringVideoFrameFactory) {
  auto video_frame_factory = encoder_->CreateVideoFrameFactory();
  ASSERT_TRUE(video_frame_factory.get());

  // The first call to |MaybeCreateFrame| will return null but post a task to
  // the encoder to initialize for the specified frame size. We then drain the
  // message loop. After that, the encoder should have initialized and we
  // request a frame again.
  ASSERT_FALSE(video_frame_factory->MaybeCreateFrame(
      gfx::Size(kVideoWidth, kVideoHeight), base::TimeDelta()));
  base::RunLoop().RunUntilIdle();
  CreateFrameAndMemsetPlane(video_frame_factory.get());

  // After a power suspension, the factory should not produce frames.
  power_source_->GenerateSuspendEvent();

  ASSERT_FALSE(video_frame_factory->MaybeCreateFrame(
      gfx::Size(kVideoWidth, kVideoHeight), base::TimeDelta()));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(video_frame_factory->MaybeCreateFrame(
      gfx::Size(kVideoWidth, kVideoHeight), base::TimeDelta()));

  // After a power resume event, the factory should produce frames right away
  // because the encoder re-initializes on its own.
  power_source_->GenerateResumeEvent();
  CreateFrameAndMemsetPlane(video_frame_factory.get());
}

TEST_F(H264VideoToolboxEncoderTest,
       CheckPowerMonitoringVideoFrameFactoryNoInitialFrame) {
  auto video_frame_factory = encoder_->CreateVideoFrameFactory();
  ASSERT_TRUE(video_frame_factory.get());

  // After a power suspension, the factory should not produce frames.
  power_source_->GenerateSuspendEvent();

  ASSERT_FALSE(video_frame_factory->MaybeCreateFrame(
      gfx::Size(kVideoWidth, kVideoHeight), base::TimeDelta()));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(video_frame_factory->MaybeCreateFrame(
      gfx::Size(kVideoWidth, kVideoHeight), base::TimeDelta()));

  // After a power resume event, the factory should produce frames right away
  // because the encoder re-initializes on its own.
  power_source_->GenerateResumeEvent();
  CreateFrameAndMemsetPlane(video_frame_factory.get());
}

}  // namespace cast
}  // namespace media
