// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_clock.h"

#include <memory>

#include "base/time/time.h"
#include "media/base/audio_timestamp_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class AudioClockTest : public testing::Test {
 public:
  AudioClockTest() { SetupClock(base::TimeDelta(), 10); }

  AudioClockTest(const AudioClockTest&) = delete;
  AudioClockTest& operator=(const AudioClockTest&) = delete;

  ~AudioClockTest() override = default;

  void WroteAudio(int frames_written,
                  int frames_requested,
                  int delay_frames,
                  double playback_rate) {
    clock_->WroteAudio(frames_written, frames_requested, delay_frames,
                       playback_rate);
  }

  void SetupClock(base::TimeDelta start_time, int sample_rate) {
    sample_rate_ = sample_rate;
    clock_ = std::make_unique<AudioClock>(start_time, sample_rate_);
  }

  int FrontTimestampInDays() { return clock_->front_timestamp().InDays(); }

  int FrontTimestampInMilliseconds() {
    return clock_->front_timestamp().InMilliseconds();
  }

  int BackTimestampInMilliseconds() {
    return clock_->back_timestamp().InMilliseconds();
  }

  int TimeUntilPlaybackInMilliseconds(int timestamp_ms) {
    return clock_->TimeUntilPlayback(base::Milliseconds(timestamp_ms))
        .InMilliseconds();
  }

  int ContiguousAudioDataBufferedInDays() {
    base::TimeDelta total, same_rate_total;
    clock_->ContiguousAudioDataBufferedForTesting(&total, &same_rate_total);
    return total.InDays();
  }

  int ContiguousAudioDataBufferedInMilliseconds() {
    base::TimeDelta total, same_rate_total;
    clock_->ContiguousAudioDataBufferedForTesting(&total, &same_rate_total);
    return total.InMilliseconds();
  }

  int ContiguousAudioDataBufferedAtSameRateInMilliseconds() {
    base::TimeDelta total, same_rate_total;
    clock_->ContiguousAudioDataBufferedForTesting(&total, &same_rate_total);
    return same_rate_total.InMilliseconds();
  }

  int sample_rate_;
  std::unique_ptr<AudioClock> clock_;
};

TEST_F(AudioClockTest, FrontTimestampStartsAtStartTimestamp) {
  base::TimeDelta expected = base::Seconds(123);
  AudioClock clock(expected, sample_rate_);

  EXPECT_EQ(expected, clock.front_timestamp());
}

TEST_F(AudioClockTest, BackTimestampStartsAtStartTimestamp) {
  base::TimeDelta expected = base::Seconds(123);
  AudioClock clock(expected, sample_rate_);

  EXPECT_EQ(expected, clock.back_timestamp());
}

TEST_F(AudioClockTest, Playback) {
  // The first time we write data we should still expect our start timestamp
  // due to delay.
  WroteAudio(10, 10, 20, 1.0);
  EXPECT_EQ(0, FrontTimestampInMilliseconds());
  EXPECT_EQ(1000, BackTimestampInMilliseconds());
  EXPECT_EQ(0, ContiguousAudioDataBufferedInMilliseconds());
  EXPECT_EQ(0, ContiguousAudioDataBufferedAtSameRateInMilliseconds());

  // The media time should remain at start timestamp as we write data.
  WroteAudio(10, 10, 20, 1.0);
  EXPECT_EQ(0, FrontTimestampInMilliseconds());
  EXPECT_EQ(2000, BackTimestampInMilliseconds());
  EXPECT_EQ(0, ContiguousAudioDataBufferedInMilliseconds());
  EXPECT_EQ(0, ContiguousAudioDataBufferedAtSameRateInMilliseconds());

  WroteAudio(10, 10, 20, 1.0);
  EXPECT_EQ(0, FrontTimestampInMilliseconds());
  EXPECT_EQ(3000, BackTimestampInMilliseconds());
  EXPECT_EQ(3000, ContiguousAudioDataBufferedInMilliseconds());
  EXPECT_EQ(3000, ContiguousAudioDataBufferedAtSameRateInMilliseconds());

  // The media time should now start advanced now that delay has been covered.
  WroteAudio(10, 10, 20, 1.0);
  EXPECT_EQ(1000, FrontTimestampInMilliseconds());
  EXPECT_EQ(4000, BackTimestampInMilliseconds());
  EXPECT_EQ(3000, ContiguousAudioDataBufferedInMilliseconds());
  EXPECT_EQ(3000, ContiguousAudioDataBufferedAtSameRateInMilliseconds());

  WroteAudio(10, 10, 20, 1.0);
  EXPECT_EQ(2000, FrontTimestampInMilliseconds());
  EXPECT_EQ(5000, BackTimestampInMilliseconds());
  EXPECT_EQ(3000, ContiguousAudioDataBufferedInMilliseconds());
  EXPECT_EQ(3000, ContiguousAudioDataBufferedAtSameRateInMilliseconds());

  // Introduce a rate change to slow down time:
  //   - Current time will advance by one second until it hits rate change
  //   - Contiguous audio data will start shrinking immediately
  WroteAudio(10, 10, 20, 0.5);
  EXPECT_EQ(3000, FrontTimestampInMilliseconds());
  EXPECT_EQ(5500, BackTimestampInMilliseconds());
  EXPECT_EQ(2500, ContiguousAudioDataBufferedInMilliseconds());
  EXPECT_EQ(2000, ContiguousAudioDataBufferedAtSameRateInMilliseconds());

  WroteAudio(10, 10, 20, 0.5);
  EXPECT_EQ(4000, FrontTimestampInMilliseconds());
  EXPECT_EQ(6000, BackTimestampInMilliseconds());
  EXPECT_EQ(2000, ContiguousAudioDataBufferedInMilliseconds());
  EXPECT_EQ(1000, ContiguousAudioDataBufferedAtSameRateInMilliseconds());

  WroteAudio(10, 10, 20, 0.5);
  EXPECT_EQ(5000, FrontTimestampInMilliseconds());
  EXPECT_EQ(6500, BackTimestampInMilliseconds());
  EXPECT_EQ(1500, ContiguousAudioDataBufferedInMilliseconds());
  EXPECT_EQ(1500, ContiguousAudioDataBufferedAtSameRateInMilliseconds());

  WroteAudio(10, 10, 20, 0.5);
  EXPECT_EQ(5500, FrontTimestampInMilliseconds());
  EXPECT_EQ(7000, BackTimestampInMilliseconds());
  EXPECT_EQ(1500, ContiguousAudioDataBufferedInMilliseconds());
  EXPECT_EQ(1500, ContiguousAudioDataBufferedAtSameRateInMilliseconds());

  // Introduce a rate change to speed up time:
  //   - Current time will advance by half a second until it hits rate change
  //   - Contiguous audio data will start growing immediately
  WroteAudio(10, 10, 20, 2);
  EXPECT_EQ(6000, FrontTimestampInMilliseconds());
  EXPECT_EQ(9000, BackTimestampInMilliseconds());
  EXPECT_EQ(3000, ContiguousAudioDataBufferedInMilliseconds());
  EXPECT_EQ(1000, ContiguousAudioDataBufferedAtSameRateInMilliseconds());

  WroteAudio(10, 10, 20, 2);
  EXPECT_EQ(6500, FrontTimestampInMilliseconds());
  EXPECT_EQ(11000, BackTimestampInMilliseconds());
  EXPECT_EQ(4500, ContiguousAudioDataBufferedInMilliseconds());
  EXPECT_EQ(500, ContiguousAudioDataBufferedAtSameRateInMilliseconds());

  WroteAudio(10, 10, 20, 2);
  EXPECT_EQ(7000, FrontTimestampInMilliseconds());
  EXPECT_EQ(13000, BackTimestampInMilliseconds());
  EXPECT_EQ(6000, ContiguousAudioDataBufferedInMilliseconds());
  EXPECT_EQ(6000, ContiguousAudioDataBufferedAtSameRateInMilliseconds());

  WroteAudio(10, 10, 20, 2);
  EXPECT_EQ(9000, FrontTimestampInMilliseconds());
  EXPECT_EQ(15000, BackTimestampInMilliseconds());
  EXPECT_EQ(6000, ContiguousAudioDataBufferedInMilliseconds());
  EXPECT_EQ(6000, ContiguousAudioDataBufferedAtSameRateInMilliseconds());

  // Write silence to simulate reaching end of stream:
  //   - Current time will advance by half a second until it hits silence
  //   - Contiguous audio data will start shrinking towards zero
  WroteAudio(0, 10, 20, 2);
  EXPECT_EQ(11000, FrontTimestampInMilliseconds());
  EXPECT_EQ(15000, BackTimestampInMilliseconds());
  EXPECT_EQ(4000, ContiguousAudioDataBufferedInMilliseconds());
  EXPECT_EQ(4000, ContiguousAudioDataBufferedAtSameRateInMilliseconds());

  WroteAudio(0, 10, 20, 2);
  EXPECT_EQ(13000, FrontTimestampInMilliseconds());
  EXPECT_EQ(15000, BackTimestampInMilliseconds());
  EXPECT_EQ(2000, ContiguousAudioDataBufferedInMilliseconds());
  EXPECT_EQ(2000, ContiguousAudioDataBufferedAtSameRateInMilliseconds());

  WroteAudio(0, 10, 20, 2);
  EXPECT_EQ(15000, FrontTimestampInMilliseconds());
  EXPECT_EQ(15000, BackTimestampInMilliseconds());
  EXPECT_EQ(0, ContiguousAudioDataBufferedInMilliseconds());
  EXPECT_EQ(0, ContiguousAudioDataBufferedAtSameRateInMilliseconds());

  // At this point media time should stop increasing.
  WroteAudio(0, 10, 20, 2);
  EXPECT_EQ(15000, FrontTimestampInMilliseconds());
  EXPECT_EQ(15000, BackTimestampInMilliseconds());
  EXPECT_EQ(0, ContiguousAudioDataBufferedInMilliseconds());
  EXPECT_EQ(0, ContiguousAudioDataBufferedAtSameRateInMilliseconds());
}

TEST_F(AudioClockTest, AlternatingAudioAndSilence) {
  // Buffer #1: [0, 1000)
  WroteAudio(10, 10, 20, 1.0);
  EXPECT_EQ(0, FrontTimestampInMilliseconds());
  EXPECT_EQ(1000, BackTimestampInMilliseconds());
  EXPECT_EQ(0, ContiguousAudioDataBufferedInMilliseconds());

  // Buffer #2: 1000ms of silence
  WroteAudio(0, 10, 20, 1.0);
  EXPECT_EQ(0, FrontTimestampInMilliseconds());
  EXPECT_EQ(1000, BackTimestampInMilliseconds());
  EXPECT_EQ(0, ContiguousAudioDataBufferedInMilliseconds());

  // Buffer #3: [1000, 2000):
  //   - Buffer #1 is at front with 1000ms of contiguous audio data
  WroteAudio(10, 10, 20, 1.0);
  EXPECT_EQ(0, FrontTimestampInMilliseconds());
  EXPECT_EQ(2000, BackTimestampInMilliseconds());
  EXPECT_EQ(1000, ContiguousAudioDataBufferedInMilliseconds());

  // Buffer #4: 1000ms of silence
  //   - Buffer #1 has been played out
  //   - Buffer #2 of silence leaves us with 0ms of contiguous audio data
  WroteAudio(0, 10, 20, 1.0);
  EXPECT_EQ(1000, FrontTimestampInMilliseconds());
  EXPECT_EQ(2000, BackTimestampInMilliseconds());
  EXPECT_EQ(0, ContiguousAudioDataBufferedInMilliseconds());

  // Buffer #5: [2000, 3000):
  //   - Buffer #3 is at front with 1000ms of contiguous audio data
  WroteAudio(10, 10, 20, 1.0);
  EXPECT_EQ(1000, FrontTimestampInMilliseconds());
  EXPECT_EQ(3000, BackTimestampInMilliseconds());
  EXPECT_EQ(1000, ContiguousAudioDataBufferedInMilliseconds());
}

TEST_F(AudioClockTest, ZeroDelay) {
  // The first time we write data we should expect the first timestamp
  // immediately.
  WroteAudio(10, 10, 0, 1.0);
  EXPECT_EQ(0, FrontTimestampInMilliseconds());
  EXPECT_EQ(1000, BackTimestampInMilliseconds());
  EXPECT_EQ(1000, ContiguousAudioDataBufferedInMilliseconds());

  // Ditto for all subsequent buffers.
  WroteAudio(10, 10, 0, 1.0);
  EXPECT_EQ(1000, FrontTimestampInMilliseconds());
  EXPECT_EQ(2000, BackTimestampInMilliseconds());
  EXPECT_EQ(1000, ContiguousAudioDataBufferedInMilliseconds());

  WroteAudio(10, 10, 0, 1.0);
  EXPECT_EQ(2000, FrontTimestampInMilliseconds());
  EXPECT_EQ(3000, BackTimestampInMilliseconds());
  EXPECT_EQ(1000, ContiguousAudioDataBufferedInMilliseconds());

  // Ditto for silence.
  WroteAudio(0, 10, 0, 1.0);
  EXPECT_EQ(3000, FrontTimestampInMilliseconds());
  EXPECT_EQ(3000, BackTimestampInMilliseconds());
  EXPECT_EQ(0, ContiguousAudioDataBufferedInMilliseconds());

  WroteAudio(0, 10, 0, 1.0);
  EXPECT_EQ(3000, FrontTimestampInMilliseconds());
  EXPECT_EQ(3000, BackTimestampInMilliseconds());
  EXPECT_EQ(0, ContiguousAudioDataBufferedInMilliseconds());
}

TEST_F(AudioClockTest, TimeUntilPlayback) {
  // Construct an audio clock with the following representation:
  //
  //    existing
  // |-  delay   -|------------------ calls to WroteAudio() ------------------|
  // +------------+---------+------------+-----------+------------+-----------+
  // | 20 silence | 10 @ 1x | 10 silence | 10 @ 0.5x | 10 silence | 10 @ 2.0x |
  // +------------+---------+------------+-----------+------------+-----------+
  // Media:       0       1000         1000        1500         1500        3500
  // Wall:      2000      3000         4000        5000         6000        7000
  WroteAudio(10, 10, 60, 1.0);
  WroteAudio(0, 10, 60, 1.0);
  WroteAudio(10, 10, 60, 0.5);
  WroteAudio(0, 10, 60, 0.5);
  WroteAudio(10, 10, 60, 2.0);
  EXPECT_EQ(0, FrontTimestampInMilliseconds());
  EXPECT_EQ(3500, BackTimestampInMilliseconds());
  EXPECT_EQ(0, ContiguousAudioDataBufferedInMilliseconds());

  // Media timestamp zero has to wait for silence to pass.
  EXPECT_EQ(2000, TimeUntilPlaybackInMilliseconds(0));

  // From then on out it's simply adding up the number of frames and taking
  // silence into account.
  EXPECT_EQ(2500, TimeUntilPlaybackInMilliseconds(500));
  EXPECT_EQ(3000, TimeUntilPlaybackInMilliseconds(1000));
  EXPECT_EQ(4500, TimeUntilPlaybackInMilliseconds(1250));
  EXPECT_EQ(5000, TimeUntilPlaybackInMilliseconds(1500));
  EXPECT_EQ(6500, TimeUntilPlaybackInMilliseconds(2500));
  EXPECT_EQ(7000, TimeUntilPlaybackInMilliseconds(3500));
}

TEST_F(AudioClockTest, SupportsYearsWorthOfAudioData) {
  // Use number of frames that would be likely to overflow 32-bit integer math.
  const int huge_amount_of_frames = std::numeric_limits<int>::max();
  const base::TimeDelta huge =
      base::Seconds(huge_amount_of_frames / sample_rate_);
  EXPECT_EQ(2485, huge.InDays());  // Just to give some context on how big...

  // Use zero delay to test calculation of current timestamp.
  WroteAudio(huge_amount_of_frames, huge_amount_of_frames, 0, 1.0);
  EXPECT_EQ(0, FrontTimestampInDays());
  EXPECT_EQ(2485, ContiguousAudioDataBufferedInDays());

  WroteAudio(huge_amount_of_frames, huge_amount_of_frames, 0, 1.0);
  EXPECT_EQ(huge.InDays(), FrontTimestampInDays());
  EXPECT_EQ(huge.InDays(), ContiguousAudioDataBufferedInDays());

  WroteAudio(huge_amount_of_frames, huge_amount_of_frames, 0, 1.0);
  EXPECT_EQ((huge * 2).InDays(), FrontTimestampInDays());
  EXPECT_EQ(huge.InDays(), ContiguousAudioDataBufferedInDays());

  WroteAudio(huge_amount_of_frames, huge_amount_of_frames, 0, 1.0);
  EXPECT_EQ((huge * 3).InDays(), FrontTimestampInDays());
  EXPECT_EQ(huge.InDays(), ContiguousAudioDataBufferedInDays());

  // Use huge delay to test calculation of buffered data.
  WroteAudio(
      huge_amount_of_frames, huge_amount_of_frames, huge_amount_of_frames, 1.0);
  EXPECT_EQ((huge * 3).InDays(), FrontTimestampInDays());
  EXPECT_EQ((huge * 2).InDays(), ContiguousAudioDataBufferedInDays());
}

TEST_F(AudioClockTest, CompensateForSuspendedWrites) {
  // Buffer 6 seconds of delay and 1 second of audio data.
  WroteAudio(10, 10, 60, 1.0);

  // Media timestamp zero has to wait for silence to pass.
  const int kBaseTimeMs = 6000;
  EXPECT_EQ(kBaseTimeMs, TimeUntilPlaybackInMilliseconds(0));

  // Elapsing frames less than we have buffered should do nothing.
  const int kDelayFrames = 2;
  for (int i = 1000; i <= kBaseTimeMs; i += 1000) {
    clock_->CompensateForSuspendedWrites(base::Milliseconds(i), kDelayFrames);
    EXPECT_EQ(kBaseTimeMs - (i - 1000), TimeUntilPlaybackInMilliseconds(0));

    // Write silence to simulate maintaining a 7s output buffer.
    WroteAudio(0, 10, 60, 1.0);
  }

  // Exhausting all frames should advance timestamps and prime the buffer with
  // our delay frames value.
  clock_->CompensateForSuspendedWrites(base::Milliseconds(7000), kDelayFrames);
  EXPECT_EQ(kDelayFrames * 100, TimeUntilPlaybackInMilliseconds(1000));
}

TEST_F(AudioClockTest, FramesToTimePrecision) {
  SetupClock(base::TimeDelta(), 48000);
  double micros_per_frame = base::Time::kMicrosecondsPerSecond / 48000.0;
  int frames_written = 0;

  // Write ~2 hours of data to clock to give any error a significant chance to
  // accumulate.
  while (clock_->back_timestamp() <= base::Hours(2)) {
    frames_written += 1024;
    WroteAudio(1024, 1024, 0, 1);
  }

  // Verify no error accumulated.
  EXPECT_EQ(std::round(frames_written * micros_per_frame),
            clock_->back_timestamp().InMicroseconds());
}

}  // namespace media
