// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/audio/audio_frame_stats_accumulator.h"

#include <stdint.h>

#include "base/time/time.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_timestamp_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

void VerifyAccumulator(const AudioFrameStatsAccumulator& accumulator,
                       uint64_t observed_frames,
                       base::TimeDelta observed_frames_duration,
                       uint64_t glitch_frames,
                       base::TimeDelta latency,
                       base::TimeDelta min_latency,
                       base::TimeDelta average_latency,
                       base::TimeDelta max_latency,
                       const media::AudioGlitchInfo& glitch_info) {
  EXPECT_EQ(accumulator.observed_frames(), observed_frames);
  EXPECT_EQ(accumulator.observed_frames_duration(), observed_frames_duration);
  EXPECT_EQ(accumulator.glitch_frames(), glitch_frames);

  EXPECT_EQ(accumulator.glitch_frames_duration(), glitch_info.duration);
  EXPECT_EQ(accumulator.glitch_event_count(), glitch_info.count);

  EXPECT_EQ(accumulator.latency(), latency);
  EXPECT_EQ(accumulator.min_latency(), min_latency);
  EXPECT_EQ(accumulator.average_latency(), average_latency);
  EXPECT_EQ(accumulator.max_latency(), max_latency);
}

TEST(AudioFrameStatsAccumulatorTest, AbsorbEmtpy) {
  AudioFrameStatsAccumulator accumulator1;
  AudioFrameStatsAccumulator accumulator2;

  accumulator1.Absorb(accumulator2);

  VerifyAccumulator(accumulator1,
                    /*observed_frames=*/0u,
                    /*observed_frames_duration=*/base::TimeDelta(),
                    /*glitch_frames=*/0u,
                    /*latency=*/base::TimeDelta(),
                    /*min_latency=*/base::TimeDelta(),
                    /*average_latency=*/base::TimeDelta(),
                    /*max_latency=*/base::TimeDelta(),
                    media::AudioGlitchInfo());

  VerifyAccumulator(accumulator2,
                    /*observed_frames=*/0u,
                    /*observed_frames_duration=*/base::TimeDelta(),
                    /*glitch_frames=*/0u,
                    /*latency=*/base::TimeDelta(),
                    /*min_latency=*/base::TimeDelta(),
                    /*average_latency=*/base::TimeDelta(),
                    /*max_latency=*/base::TimeDelta(),
                    media::AudioGlitchInfo());
}

TEST(AudioFrameStatsAccumulatorTest, Update) {
  AudioFrameStatsAccumulator accumulator;

  const int sample_rate = 48000;
  uint64_t total_frames = 0u;
  media::AudioGlitchInfo total_glitch_info;

  int frames[] = {480, 520, 400};
  media::AudioGlitchInfo glitch_info[] = {
      {},
      {.duration = base::Milliseconds(2), .count = 1},
      {.duration = base::Milliseconds(3), .count = 2}};
  base::TimeDelta latency[] = {base::Milliseconds(30), base::Milliseconds(20),
                               base::Milliseconds(70)};

  accumulator.Update(frames[0], sample_rate, latency[0], glitch_info[0]);
  total_frames += frames[0];
  total_glitch_info += glitch_info[0];

  VerifyAccumulator(
      accumulator,
      /*observed_frames=*/total_frames,
      /*observed_frames_duration=*/
      media::AudioTimestampHelper::FramesToTime(total_frames, sample_rate),
      /*glitch_frames=*/
      media::AudioTimestampHelper::TimeToFrames(total_glitch_info.duration,
                                                sample_rate),
      /*latency=*/latency[0],
      /*min_latency=*/latency[0],
      /*average_latency=*/latency[0],
      /*max_latency=*/latency[0], total_glitch_info);

  accumulator.Update(frames[1], sample_rate, latency[1], glitch_info[1]);
  total_frames += frames[1];
  total_glitch_info += glitch_info[1];

  VerifyAccumulator(
      accumulator,
      /*observed_frames=*/total_frames,
      /*observed_frames_duration=*/
      media::AudioTimestampHelper::FramesToTime(total_frames, sample_rate),
      /*glitch_frames=*/
      media::AudioTimestampHelper::TimeToFrames(total_glitch_info.duration,
                                                sample_rate),
      /*latency=*/latency[1],
      /*min_latency=*/latency[1],
      /*average_latency=*/(latency[0] * frames[0] + latency[1] * frames[1]) /
          total_frames,
      /*max_latency=*/latency[0], total_glitch_info);

  accumulator.Update(frames[2], sample_rate, latency[2], glitch_info[2]);
  total_frames += frames[2];
  total_glitch_info += glitch_info[2];

  VerifyAccumulator(
      accumulator,
      /*observed_frames=*/total_frames,
      /*observed_frames_duration=*/
      media::AudioTimestampHelper::FramesToTime(total_frames, sample_rate),
      /*glitch_frames=*/
      media::AudioTimestampHelper::TimeToFrames(total_glitch_info.duration,
                                                sample_rate),
      /*latency=*/latency[2],
      /*min_latency=*/latency[1],
      /*average_latency=*/
      (latency[0] * frames[0] + latency[1] * frames[1] +
       latency[2] * frames[2]) /
          total_frames,
      /*max_latency=*/latency[2], total_glitch_info);
}

TEST(AudioFrameStatsAccumulatorTest, Absorb) {
  AudioFrameStatsAccumulator accumulator;
  AudioFrameStatsAccumulator absorbing_accumulator;

  const int sample_rate = 48000;
  uint64_t total_frames = 0u;
  media::AudioGlitchInfo total_glitch_info;

  int frames[] = {480, 520, 400};
  media::AudioGlitchInfo glitch_info[] = {
      {.duration = base::Milliseconds(1), .count = 2},
      {.duration = base::Milliseconds(2), .count = 1},
      {.duration = base::Milliseconds(1), .count = 1}};
  base::TimeDelta latency[] = {base::Milliseconds(10), base::Milliseconds(20),
                               base::Milliseconds(30)};

  accumulator.Update(frames[0], sample_rate, latency[0], glitch_info[0]);
  total_frames += frames[0];
  total_glitch_info += glitch_info[0];

  VerifyAccumulator(
      accumulator,
      /*observed_frames=*/total_frames,
      /*observed_frames_duration=*/
      media::AudioTimestampHelper::FramesToTime(total_frames, sample_rate),
      /*glitch_frames=*/
      media::AudioTimestampHelper::TimeToFrames(total_glitch_info.duration,
                                                sample_rate),
      /*latency=*/latency[0],
      /*min_latency=*/latency[0],
      /*average_latency=*/latency[0],
      /*max_latency=*/latency[0], total_glitch_info);

  absorbing_accumulator.Absorb(accumulator);

  VerifyAccumulator(
      absorbing_accumulator,
      /*observed_frames=*/total_frames,
      /*observed_frames_duration=*/
      media::AudioTimestampHelper::FramesToTime(total_frames, sample_rate),
      /*glitch_frames=*/
      media::AudioTimestampHelper::TimeToFrames(total_glitch_info.duration,
                                                sample_rate),
      /*latency=*/latency[0],
      /*min_latency=*/latency[0],
      /*average_latency=*/latency[0],
      /*max_latency=*/latency[0], total_glitch_info);

  // Should report the last latency for all latency stats.
  VerifyAccumulator(
      accumulator,
      /*observed_frames=*/total_frames,
      /*observed_frames_duration=*/
      media::AudioTimestampHelper::FramesToTime(total_frames, sample_rate),
      /*glitch_frames=*/
      media::AudioTimestampHelper::TimeToFrames(total_glitch_info.duration,
                                                sample_rate),
      /*latency=*/latency[0],
      /*min_latency=*/latency[0],
      /*average_latency=*/latency[0],
      /*max_latency=*/latency[0], total_glitch_info);

  accumulator.Update(frames[1], sample_rate, latency[1], glitch_info[1]);
  total_frames += frames[1];
  total_glitch_info += glitch_info[1];

  accumulator.Update(frames[2], sample_rate, latency[2], glitch_info[2]);
  total_frames += frames[2];
  total_glitch_info += glitch_info[2];

  // Latency stats are reported only for the interval started after Absorb().
  VerifyAccumulator(
      accumulator,
      /*observed_frames=*/total_frames,
      /*observed_frames_duration=*/
      media::AudioTimestampHelper::FramesToTime(total_frames, sample_rate),
      /*glitch_frames=*/
      media::AudioTimestampHelper::TimeToFrames(total_glitch_info.duration,
                                                sample_rate),
      /*latency=*/latency[2],
      /*min_latency=*/latency[1],
      /*average_latency=*/(latency[1] * frames[1] + latency[2] * frames[2]) /
          (frames[1] + frames[2]),
      /*max_latency=*/latency[2], total_glitch_info);

  absorbing_accumulator.Absorb(accumulator);

  // Should combine latency stats.
  VerifyAccumulator(
      absorbing_accumulator,
      /*observed_frames=*/total_frames,
      /*observed_frames_duration=*/
      media::AudioTimestampHelper::FramesToTime(total_frames, sample_rate),
      /*glitch_frames=*/
      media::AudioTimestampHelper::TimeToFrames(total_glitch_info.duration,
                                                sample_rate),
      /*latency=*/latency[2],
      /*min_latency=*/latency[0],
      /*average_latency=*/
      (latency[0] * frames[0] + latency[1] * frames[1] +
       latency[2] * frames[2]) /
          total_frames,
      /*max_latency=*/latency[2], total_glitch_info);

  // Should report the last latency for all latency stats.
  VerifyAccumulator(
      accumulator,
      /*observed_frames=*/total_frames,
      /*observed_frames_duration=*/
      media::AudioTimestampHelper::FramesToTime(total_frames, sample_rate),
      /*glitch_frames=*/
      media::AudioTimestampHelper::TimeToFrames(total_glitch_info.duration,
                                                sample_rate),
      /*latency=*/latency[2],
      /*min_latency=*/latency[2],
      /*average_latency=*/latency[2],
      /*max_latency=*/latency[2], total_glitch_info);
}

TEST(AudioFrameStatsAccumulatorTest, UpdateDifferentSampleRates) {
  AudioFrameStatsAccumulator accumulator;

  uint64_t total_frames = 0u;
  media::AudioGlitchInfo total_glitch_info;

  int sample_rate[] = {16000, 48000};
  int frames[] = {480, 1024};
  media::AudioGlitchInfo glitch_info[] = {
      {.duration = base::Milliseconds(5), .count = 2},
      {.duration = base::Milliseconds(1), .count = 1}};
  base::TimeDelta latency[] = {base::Milliseconds(60), base::Milliseconds(70)};

  accumulator.Update(frames[0], sample_rate[0], latency[0], glitch_info[0]);
  total_frames += frames[0];
  total_glitch_info += glitch_info[0];

  VerifyAccumulator(
      accumulator,
      /*observed_frames=*/total_frames,
      /*observed_frames_duration=*/
      media::AudioTimestampHelper::FramesToTime(frames[0], sample_rate[0]),
      /*glitch_frames=*/
      media::AudioTimestampHelper::TimeToFrames(glitch_info[0].duration,
                                                sample_rate[0]),
      /*latency=*/latency[0],
      /*min_latency=*/latency[0],
      /*average_latency=*/latency[0],
      /*max_latency=*/latency[0], total_glitch_info);

  accumulator.Update(frames[1], sample_rate[1], latency[1], glitch_info[1]);
  total_frames += frames[1];
  total_glitch_info += glitch_info[1];

  VerifyAccumulator(
      accumulator,
      /*observed_frames=*/total_frames,
      /*observed_frames_duration=*/
      media::AudioTimestampHelper::FramesToTime(frames[0], sample_rate[0]) +
          media::AudioTimestampHelper::FramesToTime(frames[1], sample_rate[1]),
      /*glitch_frames=*/
      media::AudioTimestampHelper::TimeToFrames(glitch_info[0].duration,
                                                sample_rate[0]) +
          media::AudioTimestampHelper::TimeToFrames(glitch_info[1].duration,
                                                    sample_rate[1]),
      /*latency=*/latency[1],
      /*min_latency=*/latency[0],
      /*average_latency=*/(latency[0] * frames[0] + latency[1] * frames[1]) /
          total_frames,
      /*max_latency=*/latency[1], total_glitch_info);
}

}  // namespace

}  // namespace blink
