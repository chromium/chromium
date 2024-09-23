// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_TIMESTAMP_HELPER_H_
#define MEDIA_BASE_AUDIO_TIMESTAMP_HELPER_H_

#include <stdint.h>

#include <optional>

#include "base/time/time.h"
#include "media/base/media_export.h"

namespace media {

// Generates timestamps for a sequence of audio sample frames. This class should
// be used any place timestamps need to be calculated for a sequence of audio
// samples. It helps avoid timestamps inaccuracies caused by rounding/truncation
// in repeated sample count to timestamp conversions.
//
// The class is constructed with samples_per_second information so that it can
// convert audio sample frame counts into timestamps. After the object is
// constructed, SetBaseTimestamp() must be called to specify the starting
// timestamp of the audio sequence. As audio samples are received, their frame
// counts are added using AddFrames(). These frame counts are accumulated by
// this class so GetTimestamp() can be used to determine the timestamp for the
// samples that have been added. GetDuration() calculates the proper duration
// values for samples added to the current timestamp. GetFramesToTarget()
// determines the number of frames that need to be added/removed from the
// accumulated frames to reach a target timestamp.
class MEDIA_EXPORT AudioTimestampHelper {
 public:
  // Returns the time duration of the given number of frames of audio with the
  // given sample rate (in samples per second).
  static base::TimeDelta FramesToTime(int64_t frames, int samples_per_second);

  // Returns the number of frames in the given duration of audio with the given
  // sample rate (in samples per second).
  static int64_t TimeToFrames(base::TimeDelta time, int samples_per_second);

  AudioTimestampHelper() = delete;

  explicit AudioTimestampHelper(int samples_per_second);

  AudioTimestampHelper(const AudioTimestampHelper&) = delete;
  AudioTimestampHelper& operator=(const AudioTimestampHelper&) = delete;

  // Sets the base timestamp to |base_timestamp| and the sets count to 0.
  void SetBaseTimestamp(base::TimeDelta base_timestamp);

  std::optional<base::TimeDelta> base_timestamp() const {
    return base_timestamp_;
  }

  int64_t frame_count() const { return frame_count_; }

  // Adds |frame_count| to the frame counter.
  // Note: SetBaseTimestamp() must be called prior to this method.
  void AddFrames(int frame_count);

  // Get the current timestamp. This value is computed from the base_timestamp()
  // and the number of sample frames that have been added so far.
  base::TimeDelta GetTimestamp() const;

  // Gets the duration of |frame_count| frames by calculating the difference
  // between the current timestamp and what the timestamp would be if
  // |frame_count| frames were added.
  base::TimeDelta GetFrameDuration(int frame_count) const;

  // Returns the number of frames needed to reach the target timestamp.
  // Note: |target| must be >= |base_timestamp_|.
  int64_t GetFramesToTarget(base::TimeDelta target) const;

  // Clears `base_timestamp_` and `frame_count_`. SetBaseTimestamp() must be
  // called again.
  void Reset();

 private:
  base::TimeDelta ComputeTimestamp(int64_t frame_count) const;

  double microseconds_per_frame_;

  std::optional<base::TimeDelta> base_timestamp_;

  // Number of frames accumulated by AddFrames() calls.
  int64_t frame_count_;
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_TIMESTAMP_HELPER_H_
