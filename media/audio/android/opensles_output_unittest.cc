// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/opensles_output.h"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <stdint.h>

#include <memory>

#include "base/time/time.h"
#include "media/audio/mock_audio_source_callback.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::NotNull;
using ::testing::SaveArg;

namespace media {

namespace {

constexpr int kSampleRate = 48000;
constexpr int kFramesPerBuffer = 480;  // 10ms per buffer at 48kHz.

int ZeroAudioBusAndReturnFrames(base::TimeDelta /* delay */,
                                base::TimeTicks /* delay_timestamp */,
                                const AudioGlitchInfo& /* glitch_info */,
                                AudioBus* dest) {
  dest->Zero();
  return dest->frames();
}

}  // namespace

class OpenSLESOutputStreamTest : public testing::Test {
 protected:
  void SetUp() override {
    g_current_test_ = this;

    static const SLPlayItf_ kPlayItf = {
        .SetPlayState = &OpenSLESOutputStreamTest::FakeSetPlayState,
        .GetPlayState = &OpenSLESOutputStreamTest::FakeGetPlayState,
        .GetPosition = &OpenSLESOutputStreamTest::FakeGetPosition,
    };
    static const SLPlayItf_* const kPlayItfPtr = &kPlayItf;

    static const SLAndroidSimpleBufferQueueItf_ kBufferQueueItf = {
        .Enqueue = &OpenSLESOutputStreamTest::FakeEnqueue,
        .Clear = &OpenSLESOutputStreamTest::FakeClear,
        .GetState = &OpenSLESOutputStreamTest::FakeGetState,
    };
    static const SLAndroidSimpleBufferQueueItf_* const kBufferQueueItfPtr =
        &kBufferQueueItf;

    AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                           ChannelLayoutConfig::Stereo(), kSampleRate,
                           kFramesPerBuffer);
    stream_ = std::make_unique<OpenSLESOutputStream>(
        /*manager=*/nullptr, params, SL_ANDROID_STREAM_MEDIA);
    stream_->SetupAudioBuffer();
    stream_->player_ = &kPlayItfPtr;
    stream_->simple_buffer_queue_ = &kBufferQueueItfPtr;
  }

  void TearDown() override {
    if (stream_) {
      stream_->Stop();
      stream_->player_ = nullptr;
      stream_->simple_buffer_queue_ = nullptr;
      stream_.reset();
    }
    g_current_test_ = nullptr;
  }

  void SetHardwareLatency(base::TimeDelta latency) {
    stream_->hardware_latency_ = latency;
  }

  int CalculateDelayFrames(base::TimeDelta position) const {
    return stream_->CalculateDelayFrames(position);
  }

  void TriggerBufferQueueCallback() {
    OpenSLESOutputStream::SimpleBufferQueueCallback(
        stream_->simple_buffer_queue_, stream_.get());
  }

  static SLresult FakeSetPlayState(SLPlayItf /* self */, SLuint32 state) {
    g_current_test_->play_state_ = state;
    return SL_RESULT_SUCCESS;
  }

  static SLresult FakeGetPlayState(SLPlayItf /* self */, SLuint32* state) {
    *state = g_current_test_->play_state_;
    return SL_RESULT_SUCCESS;
  }

  static SLresult FakeGetPosition(SLPlayItf /* self */, SLmillisecond* msec) {
    if (g_current_test_->get_position_result_ != SL_RESULT_SUCCESS) {
      return g_current_test_->get_position_result_;
    }
    *msec = g_current_test_->position_in_ms_;
    return SL_RESULT_SUCCESS;
  }

  static SLresult FakeEnqueue(SLAndroidSimpleBufferQueueItf /* self */,
                              const void* /* buffer */,
                              SLuint32 size) {
    ++g_current_test_->enqueued_buffers_;
    g_current_test_->last_enqueued_size_ = size;
    return SL_RESULT_SUCCESS;
  }

  static SLresult FakeClear(SLAndroidSimpleBufferQueueItf /* self */) {
    g_current_test_->enqueued_buffers_ = 0;
    return SL_RESULT_SUCCESS;
  }

  static SLresult FakeGetState(SLAndroidSimpleBufferQueueItf /* self */,
                               SLAndroidSimpleBufferQueueState* state) {
    state->count = 0;
    state->index = 0;
    return SL_RESULT_SUCCESS;
  }

  static OpenSLESOutputStreamTest* g_current_test_;

  SLuint32 play_state_ = SL_PLAYSTATE_STOPPED;
  SLmillisecond position_in_ms_ = 0;
  SLresult get_position_result_ = SL_RESULT_SUCCESS;
  int enqueued_buffers_ = 0;
  SLuint32 last_enqueued_size_ = 0;

  MockAudioSourceCallback callback_;
  std::unique_ptr<OpenSLESOutputStream> stream_;
};

OpenSLESOutputStreamTest* OpenSLESOutputStreamTest::g_current_test_ = nullptr;

TEST_F(OpenSLESOutputStreamTest,
       StartAdjustsInitialBaseTimestampForHardwareLatency) {
  // Simulate a device where Start() observes GetPosition() = 100ms (e.g. a
  // reused OpenSLES player whose position was not reset on Clear()) and
  // hardware_latency_ is 40ms. Base timestamp should be initialized to
  // 100ms - 40ms = 60ms, with 1 buffer (10ms = 480 frames) queued.
  SetHardwareLatency(base::Milliseconds(40));
  position_in_ms_ = 100;
  stream_->Start(&callback_);
  EXPECT_EQ(enqueued_buffers_, 1);

  // When playback has not advanced yet (position_in_ms_ == 100ms, adjusted to
  // 60ms), all 480 queued frames (10ms) remain unplayed.
  EXPECT_EQ(CalculateDelayFrames(base::Milliseconds(100)), kFramesPerBuffer);

  // When hardware latency exceeds initial position (e.g. position_in_ms_ = 20ms
  // with 40ms latency), AdjustPositionForHardwareLatency clamps both the base
  // timestamp and subsequent positions <= 40ms to 0ms.
  stream_->Stop();
  SetHardwareLatency(base::Milliseconds(40));
  position_in_ms_ = 20;
  stream_->Start(&callback_);
  EXPECT_EQ(CalculateDelayFrames(base::Milliseconds(20)), kFramesPerBuffer);
  EXPECT_EQ(CalculateDelayFrames(base::Milliseconds(35)), kFramesPerBuffer);
}

TEST_F(OpenSLESOutputStreamTest,
       NonMonotonicPositionBelowBaseTimestampDoesNotCrash) {
  // Start with initial position 100ms and 0ms hardware latency (base_timestamp
  // = 100ms, 1 buffer of 480 frames = 10ms queued).
  position_in_ms_ = 100;
  stream_->Start(&callback_);

  // Simulate non-monotonic GetPosition() jittering backward to 80ms (< 100ms
  // base_timestamp). Previously this triggered a CHECK(target >=
  // *base_timestamp_) crash in AudioTimestampHelper::GetFramesToTarget().
  position_in_ms_ = 80;
  base::TimeDelta reported_delay;
  EXPECT_CALL(callback_, OnMoreData(_, _, AudioGlitchInfo(), NotNull()))
      .WillOnce(
          DoAll(SaveArg<0>(&reported_delay), ZeroAudioBusAndReturnFrames));

  TriggerBufferQueueCallback();

  // Target position is clamped to base_timestamp (100ms), so delay equals the
  // 1 buffer (10ms) queued during Start().
  EXPECT_EQ(reported_delay, base::Milliseconds(10));
  EXPECT_EQ(enqueued_buffers_, 2);
}

TEST_F(OpenSLESOutputStreamTest, ClampsDelayFramesToValidBounds) {
  position_in_ms_ = 0;
  stream_->Start(&callback_);

  // 1 buffer (480 frames = 10ms) has been added during Start(). If GetPosition
  // jumps ahead to 500ms due to clock skew, GetFramesToTarget() is positive
  // and -GetFramesToTarget() is negative; CalculateDelayFrames must clamp to 0.
  EXPECT_EQ(CalculateDelayFrames(base::Milliseconds(500)), 0);

  // Enqueue two more buffers via callbacks at position 0ms so total frames
  // added is 3 * kFramesPerBuffer (exceeding kMaxNumOfBuffersInQueue = 2).
  // CalculateDelayFrames must clamp to kMaxNumOfBuffersInQueue *
  // kFramesPerBuffer (960 frames = 20ms).
  EXPECT_CALL(callback_, OnMoreData(_, _, AudioGlitchInfo(), NotNull()))
      .Times(2)
      .WillRepeatedly(ZeroAudioBusAndReturnFrames);
  TriggerBufferQueueCallback();
  TriggerBufferQueueCallback();

  EXPECT_EQ(CalculateDelayFrames(base::Milliseconds(0)),
            OpenSLESOutputStream::kMaxNumOfBuffersInQueue * kFramesPerBuffer);
}

TEST_F(OpenSLESOutputStreamTest,
       GetPositionFailureFallsBackToZeroDelayAndContinuesPlayback) {
  position_in_ms_ = 0;
  stream_->Start(&callback_);
  ASSERT_EQ(enqueued_buffers_, 1);

  // Simulate a transient GetPosition() failure during the buffer queue
  // callback. FillBufferQueueLocked() must fall back to zero delay and still
  // pull and enqueue the next buffer so the OpenSLES queue does not starve.
  get_position_result_ = SL_RESULT_INTERNAL_ERROR;
  base::TimeDelta reported_delay = base::Milliseconds(999);
  EXPECT_CALL(callback_, OnMoreData(_, _, AudioGlitchInfo(), NotNull()))
      .WillOnce(
          DoAll(SaveArg<0>(&reported_delay), ZeroAudioBusAndReturnFrames));

  TriggerBufferQueueCallback();

  EXPECT_EQ(reported_delay, base::TimeDelta());
  EXPECT_EQ(enqueued_buffers_, 2);
}

}  // namespace media
