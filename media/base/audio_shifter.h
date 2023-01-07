// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_SHIFTER_H_
#define MEDIA_BASE_AUDIO_SHIFTER_H_

#include <stddef.h>

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/multi_channel_resampler.h"

namespace media {

class AudioBus;
class ClockSmoother;

// This class works like a buffer between a push based audio source
// and a pull-based audio sink. The source and sink should operate
// at nominally the same rate, but since they may run on different
// hardware clocks, the rate may differ a little. If left unchecked,
// this difference will first cause lip sync issues between audio
// and video and eventually it will cause buffer overruns/underruns.
// This class solves all that by dynamically resampling the audio
// so that both input and output sources are happy.
//
// A note about TimeTicks. The playout_time specified in Push and
// Pull calls must come from the same timeline. That timeline can
// be anything you choose as it is never compared to any real-world
// clocks, but they must come from the same clock. Specifically,
// specifying samples / rate as the playout time in Push() or Pull()
// will NOT work.
//
class MEDIA_EXPORT AudioShifter {
 public:
  // |max_buffer_size| is how much audio we are allowed to buffer.
  // Often, this can be set fairly large as Push() will limit the
  // size when it specifies when to play the audio.
  // |clock_accuracy| is used to determine if a skip has occurred
  // in the audio (as opposed to an inaccuracy in the timestamp.)
  // It also limits the smallest amount of buffering allowed.
  // |adjustement_time| specifies how long time should be used
  // to adjust the audio. This should normally at least a few
  // seconds. The larger the value, the smoother and less audible
  // the transitions will be. (But it means that perfect audio
  // sync will take longer to achieve.)
  // |rate| is audio frames per second, eg 48000.
  // |channels| is number of channels in input and output audio.
  // TODO(hubbe): Allow input rate and output rate to be different
  // since we're going to be resampling anyways.
  AudioShifter(base::TimeDelta max_buffer_size,
               base::TimeDelta clock_accuracy,
               base::TimeDelta adjustment_time,
               int rate,
               int channels);
  ~AudioShifter();

  int sample_rate() const { return rate_; }
  int channels() const { return channels_; }

  // Push Audio into the shifter. All inputs must have the same number of
  // channels, but bus size can vary. The playout time can be noisy and
  // does not have to line up perfectly with the number of samples pushed
  // so far. However, the playout_time in Push calls and Pull calls must
  // not diverge over time.
  // Given audio from an a microphone, a reasonable way to calculate
  // playout_time would be now + 30ms.
  // Ideally playout_time is some time in the future, in which case
  // the samples will be buffered until the appropriate time. If
  // playout_time is in the past, everything will still work, and we'll
  // try to keep the buffering to a minimum.
  void Push(std::unique_ptr<AudioBus> input, base::TimeTicks playout_time);

  // Fills out |output| with samples. Tries to stretch/shrink the audio
  // to compensate for drift between input and output.
  // If called from an output device data pull, a reasonable way to
  // calculate playout_time would be now + audio pipeline delay.
  void Pull(AudioBus* output, base::TimeTicks playout_time);

  int frames_pushed_for_testing() { return frames_pushed_for_testing_; }

 private:
  struct AudioQueueEntry {
    AudioQueueEntry(base::TimeTicks target_playout_time,
                    std::unique_ptr<AudioBus> audio);
    AudioQueueEntry(AudioQueueEntry&& other);
    ~AudioQueueEntry();
    base::TimeTicks target_playout_time;
    std::unique_ptr<AudioBus> audio;
  };

  void Zero(AudioBus* output);
  void ResamplerCallback(int frame_delay, AudioBus* destination);

  // Set from constructor.
  const base::TimeDelta max_buffer_size_;
  const base::TimeDelta clock_accuracy_;
  const base::TimeDelta adjustment_time_;
  // Kept as a double to make it easier to preserve precision in frame count ->
  // total time conversions .
  const double rate_;
  const int channels_;

  // The clock smoothers are used to smooth out timestamps
  // and adjust for drift and inaccurate clocks.
  std::unique_ptr<ClockSmoother> input_clock_smoother_;
  std::unique_ptr<ClockSmoother> output_clock_smoother_;

  // Are we currently outputting data?
  bool running_;

  // Number of frames already consumed from |queue_|.
  size_t position_ = 0;

  // Queue of data provided to us.
  base::circular_deque<AudioQueueEntry> queue_;

  // Timestamp from last Pull() call.
  base::TimeTicks previous_playout_time_;

  // Number of frames requested in last Pull call.
  int previous_requested_samples_ = 0;

  // Timestamp at the end of last audio bus
  // consumed by resampler.
  base::TimeTicks end_of_last_consumed_audiobus_;

  // If Push() timestamps are in the past, we have to decide the playout delay
  // ourselves. The delay is then stored here.
  base::TimeDelta bias_;

  // Resampler.
  MultiChannelResampler resampler_;

  // Current resampler ratio.
  double current_ratio_ = 1.0;

  int frames_pushed_for_testing_ = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_SHIFTER_H_
