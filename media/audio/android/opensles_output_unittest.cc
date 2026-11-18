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
    stream_->SetHardwareLatency(latency);
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
       PreservesHardwareLatencyWhenInitialPositionIsNonZero) {
  // Simulate a device where Start() observes GetPosition() = 100ms (e.g. a
  // reused OpenSLES player whose position was not reset on Clear()) and
  // hardware_latency_ is 40ms (1920 frames = 4 * kFramesPerBuffer).
  constexpr int kHardwareLatencyFrames = 4 * kFramesPerBuffer;
  SetHardwareLatency(base::Milliseconds(40));
  position_in_ms_ = 100;
  stream_->Start(&callback_);
  EXPECT_EQ(enqueued_buffers_, 1);

  // While playback has not advanced past base_timestamp + hardware_latency_
  // (140ms), AdjustPositionForHardwareLatency(position) < 100ms is clamped to
  // base_timestamp (100ms) without crashing. At position = 100ms, the 1 buffer
  // (480 frames = 10ms) queued during Start() remains unplayed.
  EXPECT_EQ(CalculateDelayFrames(base::Milliseconds(100)), kFramesPerBuffer);

  // Advance 5 callbacks (50ms = 2400 frames filled, total queued = 60ms) while
  // GetPosition() advances by 50ms to 150ms (1 buffer of 10ms unconsumed in
  // the OpenSLES queue). In steady state (position > 140ms), the reported
  // delay must include both the 1 queued buffer (10ms) and the full 40ms of
  // hardware_latency_ (total 50ms = 2400 frames).
  EXPECT_CALL(callback_, OnMoreData(_, _, AudioGlitchInfo(), NotNull()))
      .Times(5)
      .WillRepeatedly(ZeroAudioBusAndReturnFrames);
  for (int i = 1; i <= 5; ++i) {
    position_in_ms_ = 100 + i * 10;
    TriggerBufferQueueCallback();
  }
  EXPECT_EQ(CalculateDelayFrames(base::Milliseconds(150)),
            kFramesPerBuffer + kHardwareLatencyFrames);

  // Repeat when hardware latency exceeds initial position (position_in_ms_ =
  // 20ms with 40ms latency). Positions <= 60ms clamp to base_timestamp (20ms),
  // and steady-state positions (> 60ms) preserve the full 40ms hardware
  // latency.
  stream_->Stop();
  SetHardwareLatency(base::Milliseconds(40));
  position_in_ms_ = 20;
  stream_->Start(&callback_);
  EXPECT_EQ(CalculateDelayFrames(base::Milliseconds(20)), kFramesPerBuffer);
  EXPECT_EQ(CalculateDelayFrames(base::Milliseconds(35)), kFramesPerBuffer);

  EXPECT_CALL(callback_, OnMoreData(_, _, AudioGlitchInfo(), NotNull()))
      .Times(5)
      .WillRepeatedly(ZeroAudioBusAndReturnFrames);
  for (int i = 1; i <= 5; ++i) {
    position_in_ms_ = 20 + i * 10;
    TriggerBufferQueueCallback();
  }
  EXPECT_EQ(CalculateDelayFrames(base::Milliseconds(70)),
            kFramesPerBuffer + kHardwareLatencyFrames);

  // Negative hardware latency must be sanitized to zero so it neither shifts
  // target positions forward nor shrinks max_delay_frames.
  stream_->Stop();
  SetHardwareLatency(base::Milliseconds(-50));
  position_in_ms_ = 0;
  stream_->Start(&callback_);
  EXPECT_EQ(CalculateDelayFrames(base::Milliseconds(0)), kFramesPerBuffer);
}

TEST_F(OpenSLESOutputStreamTest,
       IncludesHardwareLatencyExceedingBufferQueueCapacity) {
  // Simulate an Android TV device with a deep-buffer HAL (e.g. Dolby MS12)
  // reporting 350ms of hardware output latency (16,800 frames at 48kHz), which
  // is much larger than the 2-buffer OpenSLES queue capacity (960 frames =
  // 20ms).
  constexpr base::TimeDelta kHalLatency = base::Milliseconds(350);
  constexpr int kHalLatencyFrames = 35 * kFramesPerBuffer;  // 16,800 frames.
  SetHardwareLatency(kHalLatency);
  position_in_ms_ = 0;
  stream_->Start(&callback_);

  // Enqueue 35 buffers (350ms) as each 10ms buffer is consumed into the HAL
  // pipeline (total 36 buffers = 360ms written, GetPosition() = 350ms).
  EXPECT_CALL(callback_, OnMoreData(_, _, AudioGlitchInfo(), NotNull()))
      .Times(35)
      .WillRepeatedly(ZeroAudioBusAndReturnFrames);
  for (int i = 1; i <= 35; ++i) {
    position_in_ms_ = i * 10;
    TriggerBufferQueueCallback();
  }

  // When the 36th buffer is consumed from the OpenSLES queue (position =
  // 360ms), 360ms has been written prior to OnMoreData and
  // AdjustPositionForHardwareLatency(360ms) = 10ms, so 350ms (16,800 frames)
  // remains in the HAL pipeline at OnMoreData, and 360ms (17,280 frames =
  // kFramesPerBuffer + kHalLatencyFrames) once the callback enqueues the new
  // buffer. Verify that CalculateDelayFrames does not clamp this down to 20ms
  // (960 frames).
  position_in_ms_ = 360;
  base::TimeDelta reported_delay;
  EXPECT_CALL(callback_, OnMoreData(_, _, AudioGlitchInfo(), NotNull()))
      .WillOnce(
          DoAll(SaveArg<0>(&reported_delay), ZeroAudioBusAndReturnFrames));
  TriggerBufferQueueCallback();
  EXPECT_EQ(reported_delay, kHalLatency);
  EXPECT_EQ(CalculateDelayFrames(base::Milliseconds(360)),
            kFramesPerBuffer + kHalLatencyFrames);

  // Enqueue two more buffers without GetPosition() advancing (total 3 buffers
  // queued above position, exceeding kMaxNumOfBuffersInQueue = 2). The delay
  // must clamp to kMaxNumOfBuffersInQueue * kFramesPerBuffer +
  // kHalLatencyFrames (370ms = 17,760 frames).
  EXPECT_CALL(callback_, OnMoreData(_, _, AudioGlitchInfo(), NotNull()))
      .Times(2)
      .WillRepeatedly(ZeroAudioBusAndReturnFrames);
  TriggerBufferQueueCallback();
  TriggerBufferQueueCallback();
  EXPECT_EQ(CalculateDelayFrames(base::Milliseconds(360)),
            OpenSLESOutputStream::kMaxNumOfBuffersInQueue * kFramesPerBuffer +
                kHalLatencyFrames);
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
