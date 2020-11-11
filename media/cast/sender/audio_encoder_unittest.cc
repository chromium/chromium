// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/audio_encoder.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <sstream>
#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "media/base/audio_bus.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "media/base/media.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/common/rtp_time.h"
#include "media/cast/test/utility/audio_utility.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

static const int kNumChannels = 2;

namespace {

class TestEncodedAudioFrameReceiver {
 public:
  TestEncodedAudioFrameReceiver() : frames_received_(0) {}
  virtual ~TestEncodedAudioFrameReceiver() = default;

  int frames_received() const { return frames_received_; }

  void SetCaptureTimeBounds(const base::TimeTicks& lower_bound,
                            const base::TimeTicks& upper_bound) {
    lower_bound_ = lower_bound;
    upper_bound_ = upper_bound;
  }

  void SetSamplesPerFrame(int samples_per_frame) {
    samples_per_frame_ = samples_per_frame;
  }

  void FrameEncoded(std::unique_ptr<SenderEncodedFrame> encoded_frame,
                    int samples_skipped) {
    EXPECT_EQ(encoded_frame->dependency, EncodedFrame::KEY);
    EXPECT_EQ(frames_received_, encoded_frame->frame_id - FrameId::first());
    EXPECT_EQ(encoded_frame->frame_id, encoded_frame->referenced_frame_id);
    // RTP timestamps should be monotonically increasing and integer multiples
    // of the fixed frame size.
    EXPECT_LE(rtp_lower_bound_, encoded_frame->rtp_timestamp);
    rtp_lower_bound_ = encoded_frame->rtp_timestamp;
    EXPECT_EQ(RtpTimeDelta(), (encoded_frame->rtp_timestamp - RtpTimeTicks()) %
                                  RtpTimeDelta::FromTicks(samples_per_frame_));
    EXPECT_TRUE(!encoded_frame->data.empty());

    EXPECT_LE(lower_bound_, encoded_frame->reference_time);
    lower_bound_ = encoded_frame->reference_time;
    EXPECT_GT(upper_bound_, encoded_frame->reference_time);

    EXPECT_LE(0.0, encoded_frame->encoder_utilization);
    EXPECT_EQ(-1.0, encoded_frame->lossy_utilization);

    ++frames_received_;
  }

 private:
  int frames_received_;
  RtpTimeTicks rtp_lower_bound_;
  int samples_per_frame_;
  base::TimeTicks lower_bound_;
  base::TimeTicks upper_bound_;

  DISALLOW_COPY_AND_ASSIGN(TestEncodedAudioFrameReceiver);
};

struct TestScenario {
  const int64_t* durations_in_ms;
  size_t num_durations;

  TestScenario(const int64_t* d, size_t n)
      : durations_in_ms(d), num_durations(n) {}

  std::string ToString() const {
    std::ostringstream out;
    for (size_t i = 0; i < num_durations; ++i) {
      if (i > 0)
        out << ", ";
      out << durations_in_ms[i];
    }
    return out.str();
  }
};

}  // namespace

class AudioEncoderTest : public ::testing::TestWithParam<TestScenario> {
 public:
  AudioEncoderTest() {
    InitializeMediaLibrary();
    testing_clock_.Advance(base::TimeTicks::Now() - base::TimeTicks());
  }

  void SetUp() final {
    task_runner_ = new FakeSingleThreadTaskRunner(&testing_clock_);
    cast_environment_ = new CastEnvironment(&testing_clock_, task_runner_,
                                            task_runner_, task_runner_);
  }

  virtual ~AudioEncoderTest() = default;

  void RunTestForCodec(Codec codec) {
    const TestScenario& scenario = GetParam();
    SCOPED_TRACE(::testing::Message() << "Durations: " << scenario.ToString());

    CreateObjectsForCodec(codec);

    const base::TimeDelta frame_duration = audio_encoder_->GetFrameDuration();

    for (size_t i = 0; i < scenario.num_durations; ++i) {
      const bool simulate_missing_data = scenario.durations_in_ms[i] < 0;
      const base::TimeDelta duration = base::TimeDelta::FromMilliseconds(
          std::abs(scenario.durations_in_ms[i]));
      receiver_->SetCaptureTimeBounds(
          testing_clock_.NowTicks() - frame_duration,
          testing_clock_.NowTicks() + duration);
      if (simulate_missing_data) {
        task_runner_->RunTasks();
        testing_clock_.Advance(duration);
      } else {
        audio_encoder_->InsertAudio(audio_bus_factory_->NextAudioBus(duration),
                                    testing_clock_.NowTicks());
        task_runner_->RunTasks();
        testing_clock_.Advance(duration);
      }
    }

    DVLOG(1) << "Received " << receiver_->frames_received()
             << " frames for this test run: " << scenario.ToString();
  }

 private:
  void CreateObjectsForCodec(Codec codec) {
    audio_bus_factory_.reset(
        new TestAudioBusFactory(kNumChannels,
                                kDefaultAudioSamplingRate,
                                TestAudioBusFactory::kMiddleANoteFreq,
                                0.5f));

    receiver_.reset(new TestEncodedAudioFrameReceiver());

    audio_encoder_ = std::make_unique<AudioEncoder>(
        cast_environment_, kNumChannels, kDefaultAudioSamplingRate,
        kDefaultAudioEncoderBitrate, codec,
        base::BindRepeating(&TestEncodedAudioFrameReceiver::FrameEncoded,
                            base::Unretained(receiver_.get())));

    receiver_->SetSamplesPerFrame(audio_encoder_->GetSamplesPerFrame());
  }

  base::SimpleTestTickClock testing_clock_;
  scoped_refptr<FakeSingleThreadTaskRunner> task_runner_;
  std::unique_ptr<TestAudioBusFactory> audio_bus_factory_;
  std::unique_ptr<TestEncodedAudioFrameReceiver> receiver_;
  std::unique_ptr<AudioEncoder> audio_encoder_;
  scoped_refptr<CastEnvironment> cast_environment_;

  DISALLOW_COPY_AND_ASSIGN(AudioEncoderTest);
};

TEST_P(AudioEncoderTest, EncodeOpus) {
  RunTestForCodec(CODEC_AUDIO_OPUS);
}

TEST_P(AudioEncoderTest, EncodePcm16) {
  RunTestForCodec(CODEC_AUDIO_PCM16);
}

#if defined(OS_MAC)
TEST_P(AudioEncoderTest, EncodeAac) {
  RunTestForCodec(CODEC_AUDIO_AAC);
}
#endif

static const int64_t kOneCall_3Millis[] = {3};
static const int64_t kOneCall_10Millis[] = {10};
static const int64_t kOneCall_13Millis[] = {13};
static const int64_t kOneCall_20Millis[] = {20};

static const int64_t kTwoCalls_3Millis[] = {3, 3};
static const int64_t kTwoCalls_10Millis[] = {10, 10};
static const int64_t kTwoCalls_Mixed1[] = {3, 10};
static const int64_t kTwoCalls_Mixed2[] = {10, 3};
static const int64_t kTwoCalls_Mixed3[] = {3, 17};
static const int64_t kTwoCalls_Mixed4[] = {17, 3};

static const int64_t kManyCalls_3Millis[] = {3, 3, 3, 3, 3, 3, 3, 3,
                                             3, 3, 3, 3, 3, 3, 3};
static const int64_t kManyCalls_10Millis[] = {10, 10, 10, 10, 10, 10, 10, 10,
                                              10, 10, 10, 10, 10, 10, 10};
static const int64_t kManyCalls_Mixed1[] = {3,  10, 3,  10, 3,  10, 3,  10, 3,
                                            10, 3,  10, 3,  10, 3,  10, 3,  10};
static const int64_t kManyCalls_Mixed2[] = {10, 3, 10, 3, 10, 3, 10, 3, 10, 3,
                                            10, 3, 10, 3, 10, 3, 10, 3, 10, 3};
static const int64_t kManyCalls_Mixed3[] = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8,
                                            9, 7, 9, 3, 2, 3, 8, 4, 6, 2, 6, 4};
static const int64_t kManyCalls_Mixed4[] = {31, 4, 15, 9,  26, 53, 5,  8, 9,
                                            7,  9, 32, 38, 4,  62, 64, 3};
static const int64_t kManyCalls_Mixed5[] = {3, 14, 15, 9, 26, 53, 58, 9, 7,
                                            9, 3,  23, 8, 4,  6,  2,  6, 43};

static const int64_t kOneBigUnderrun[] = {10, 10, 10, 10, -1000, 10, 10, 10};
static const int64_t kTwoBigUnderruns[] = {10, 10, 10,    10, -712, 10,
                                           10, 10, -1311, 10, 10,   10};
static const int64_t kMixedUnderruns[] = {31, -64, 4, 15, 9,  26, -53, 5,   8,
                                          -9, 7,   9, 32, 38, -4, 62,  -64, 3};

INSTANTIATE_TEST_SUITE_P(
    AudioEncoderTestScenarios,
    AudioEncoderTest,
    ::testing::Values(
        TestScenario(kOneCall_3Millis, base::size(kOneCall_3Millis)),
        TestScenario(kOneCall_10Millis, base::size(kOneCall_10Millis)),
        TestScenario(kOneCall_13Millis, base::size(kOneCall_13Millis)),
        TestScenario(kOneCall_20Millis, base::size(kOneCall_20Millis)),
        TestScenario(kTwoCalls_3Millis, base::size(kTwoCalls_3Millis)),
        TestScenario(kTwoCalls_10Millis, base::size(kTwoCalls_10Millis)),
        TestScenario(kTwoCalls_Mixed1, base::size(kTwoCalls_Mixed1)),
        TestScenario(kTwoCalls_Mixed2, base::size(kTwoCalls_Mixed2)),
        TestScenario(kTwoCalls_Mixed3, base::size(kTwoCalls_Mixed3)),
        TestScenario(kTwoCalls_Mixed4, base::size(kTwoCalls_Mixed4)),
        TestScenario(kManyCalls_3Millis, base::size(kManyCalls_3Millis)),
        TestScenario(kManyCalls_10Millis, base::size(kManyCalls_10Millis)),
        TestScenario(kManyCalls_Mixed1, base::size(kManyCalls_Mixed1)),
        TestScenario(kManyCalls_Mixed2, base::size(kManyCalls_Mixed2)),
        TestScenario(kManyCalls_Mixed3, base::size(kManyCalls_Mixed3)),
        TestScenario(kManyCalls_Mixed4, base::size(kManyCalls_Mixed4)),
        TestScenario(kManyCalls_Mixed5, base::size(kManyCalls_Mixed5)),
        TestScenario(kOneBigUnderrun, base::size(kOneBigUnderrun)),
        TestScenario(kTwoBigUnderruns, base::size(kTwoBigUnderruns)),
        TestScenario(kMixedUnderruns, base::size(kMixedUnderruns))));

}  // namespace cast
}  // namespace media
