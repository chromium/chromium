// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_TIME_SOURCE_H_
#define MEDIA_BASE_TIME_SOURCE_H_

#include <vector>

#include "base/callback.h"
#include "base/time/time.h"
#include "media/base/media_export.h"

namespace media {

// A TimeSource is capable of providing the current media time.
class MEDIA_EXPORT TimeSource {
 public:
  // Helper alias for converting media timestamps into a wall clock timestamps.
  using WallClockTimeCB =
      base::RepeatingCallback<bool(const std::vector<base::TimeDelta>&,
                                   std::vector<base::TimeTicks>*)>;

  TimeSource() {}
  virtual ~TimeSource() {}

  // Signal the time source to start ticking. It is expected that values from
  // CurrentMediaTime() will start increasing.
  virtual void StartTicking() = 0;

  // Signal the time source to stop ticking. It is expected that values from
  // CurrentMediaTime() will remain constant.
  virtual void StopTicking() = 0;

  // Updates the current playback rate. It is expected that values from
  // CurrentMediaTime() will eventually reflect the new playback rate (e.g., the
  // media time will advance at half speed if the rate was set to 0.5).
  virtual void SetPlaybackRate(double playback_rate) = 0;

  // Sets the media time to start ticking from. Only valid to call while the
  // time source is not ticking.
  virtual void SetMediaTime(base::TimeDelta time) = 0;

  // Returns the current media timestamp relative to the timestamp set by
  // SetMediaTime().
  //
  // Values returned are intended for informational purposes, such as displaying
  // UI with the current minute and second count. While it is guaranteed values
  // will never go backwards, the frequency at which they update may be low.
  virtual base::TimeDelta CurrentMediaTime() = 0;

  // Converts a vector of media timestamps into a vector of wall clock times; if
  // the media time is stopped, returns false, otherwise returns true. Even if
  // time is stopped, timestamps will be converted.
  //
  // Passing an empty |media_timestamps| vector will return the last known media
  // time as a wall clock time. After SetMediaTime() and prior to StartTicking()
  // the returned wall clock time must be zero.
  //
  // Within a single call to GetWallClockTimes() the returned wall clock times
  // are a strictly increasing function of the given media times. There is no
  // such guarantee between calls though; e.g., playback rate or audio delay may
  // change on other threads within the pipeline.
  //
  // Each timestamp converted from |media_timestamps| will be pushed into
  // |wall_clock_times| such that after all timestamps are converted, the two
  // vectors are parallel (media_timestamps[i] -> wall_clock_times[i]).
  //
  // |media_timestamps| values too far ahead of the current media time will
  // be converted to an estimated value; as such, these values may go backwards
  // in time slightly between calls to GetWallClockTimes().
  //
  // |media_timestamps| values behind the current media time may be
  // significantly incorrect if the playback rate has changed recently. The only
  // guarantee is that the returned time will be less than the current wall
  // clock time.
  virtual bool GetWallClockTimes(
      const std::vector<base::TimeDelta>& media_timestamps,
      std::vector<base::TimeTicks>* wall_clock_times) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_TIME_SOURCE_H_
