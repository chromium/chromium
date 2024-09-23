// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_AUDIO_CLOCK_H_
#define MEDIA_FILTERS_AUDIO_CLOCK_H_

#include <stdint.h>

#include <cmath>

#include "base/containers/circular_deque.h"
#include "base/time/time.h"
#include "media/base/media_export.h"

namespace media {

// Models a queue of buffered audio in a playback pipeline for use with
// estimating the amount of delay in wall clock time. Takes changes in playback
// rate into account to handle scenarios where multiple rates may be present in
// a playback pipeline with large delay.
//
//
// USAGE
//
// Prior to starting audio playback, construct an AudioClock with an initial
// media timestamp and a sample rate matching the sample rate the audio device
// was opened at.
//
// Each time the audio rendering callback is executed, call WroteAudio() once
// (and only once!) containing information on what was written:
//   1) How many frames of audio data requested
//   2) How many frames of audio data provided
//   3) The playback rate of the audio data provided
//   4) The current amount of delay
//
// After a call to WroteAudio(), clients can inspect the resulting media
// timestamp. This can be used for UI purposes, synchronizing video, etc...
//
//
// DETAILS
//
// Silence (whether caused by the initial audio delay or failing to write the
// amount of requested frames due to underflow) is also modeled and will cause
// the media timestamp to stop increasing until all known silence has been
// played. AudioClock's model is initialized with silence during the first call
// to WroteAudio() using the delay value.
//
// Playback rates are tracked for translating frame durations into media
// durations. Since silence doesn't affect media timestamps, it also isn't
// affected by playback rates.
class MEDIA_EXPORT AudioClock {
 public:
  AudioClock(base::TimeDelta start_timestamp, int sample_rate);

  AudioClock(const AudioClock&) = delete;
  AudioClock& operator=(const AudioClock&) = delete;

  ~AudioClock();

  // |frames_written| amount of audio data scaled to |playback_rate| written.
  // |frames_requested| amount of audio data requested by hardware.
  // |delay_frames| is the current amount of hardware delay.
  void WroteAudio(int frames_written,
                  int frames_requested,
                  int delay_frames,
                  double playback_rate);

  // If WroteAudio() calls are suspended (i.e. due to playback being paused) the
  // AudioClock will not properly advance time (even though all data up until
  // back_timestamp() will playout on the physical device).
  //
  // To compensate for this, when calls resume, before the next WroteAudio(),
  // callers should call CompensateForSuspendedWrites() to advance the clock for
  // audio which continued playing out while WroteAudio() calls were suspended.
  //
  // |delay_frames| must be provided to properly prime the clock to compensate
  // for a new initial delay.
  void CompensateForSuspendedWrites(base::TimeDelta elapsed, int delay_frames);

  // Returns the bounds of media data currently buffered by the audio hardware,
  // taking silence and changes in playback rate into account. Buffered audio
  // structure and timestamps are updated with every call to WroteAudio().
  //
  //  start_timestamp = 1000 ms                           sample_rate = 40 Hz
  // +-----------------------+-----------------------+-----------------------+
  // |   10 frames silence   |   20 frames @ 1.0x    |   20 frames @ 0.5x    |
  // |      = 250 ms (wall)  |      = 500 ms (wall)  |      = 500 ms (wall)  |
  // |      =   0 ms (media) |      = 500 ms (media) |      = 250 ms (media) |
  // +-----------------------+-----------------------+-----------------------+
  // ^                                                                       ^
  // front_timestamp() is equal to               back_timestamp() is equal to
  // |start_timestamp| since no                  amount of media frames tracked
  // media data has been played yet.             by AudioClock, which would be
  //                                             1000 + 500 + 250 = 1750 ms.
  base::TimeDelta front_timestamp() const {
    return base::Microseconds(std::round(front_timestamp_micros_));
  }
  base::TimeDelta back_timestamp() const {
    return base::Microseconds(std::round(back_timestamp_micros_));
  }

  // Returns the amount of wall time until |timestamp| will be played by the
  // audio hardware.
  //
  // |timestamp| must be within front_timestamp() and back_timestamp().
  base::TimeDelta TimeUntilPlayback(base::TimeDelta timestamp) const;

  void ContiguousAudioDataBufferedForTesting(
      base::TimeDelta* total,
      base::TimeDelta* same_rate_total) const;

 private:
  // Even with a ridiculously high sample rate of 256kHz, using 64 bits will
  // permit tracking up to 416999965 days worth of time (that's 1141 millennia).
  //
  // 32 bits on the other hand would top out at measly 2 hours and 20 minutes.
  struct AudioData {
    AudioData(int64_t frames, double playback_rate);

    int64_t frames;
    double playback_rate;
  };

  // Helpers for operating on |buffered_|.
  void PushBufferedAudioData(int64_t frames, double playback_rate);
  void PopBufferedAudioData(int64_t frames);
  double ComputeBufferedMediaDurationMicros() const;

  const base::TimeDelta start_timestamp_;
  const double microseconds_per_frame_;

  base::circular_deque<AudioData> buffered_;
  int64_t total_buffered_frames_;

  // Use double rather than TimeDelta to avoid loss of partial microseconds when
  // converting between frames-written/delayed and time-passed (see conversion
  // in WroteAudio()). Particularly for |back_timestamp|, which accumulates more
  // time with each call to WroteAudio(), the loss of precision can accumulate
  // to create noticeable audio/video sync drift for longer (2-3 hr) videos.
  // See http://crbug.com/564604.
  double front_timestamp_micros_;
  double back_timestamp_micros_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_CLOCK_H_
