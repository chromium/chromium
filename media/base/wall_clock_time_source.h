// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WALL_CLOCK_TIME_SOURCE_H_
#define MEDIA_BASE_WALL_CLOCK_TIME_SOURCE_H_

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/default_tick_clock.h"
#include "media/base/media_export.h"
#include "media/base/time_source.h"

namespace media {

// A time source that uses interpolation based on the system clock.
class MEDIA_EXPORT WallClockTimeSource : public TimeSource {
 public:
  WallClockTimeSource();

  WallClockTimeSource(const WallClockTimeSource&) = delete;
  WallClockTimeSource& operator=(const WallClockTimeSource&) = delete;

  ~WallClockTimeSource() override;

  // TimeSource implementation.
  void StartTicking() override;
  void StopTicking() override;
  void SetPlaybackRate(double playback_rate) override;
  void SetMediaTime(base::TimeDelta time) override;
  base::TimeDelta CurrentMediaTime() override;
  bool GetWallClockTimes(
      const std::vector<base::TimeDelta>& media_timestamps,
      std::vector<base::TimeTicks>* wall_clock_times) override;

 private:
  friend class VideoRendererAlgorithmTest;
  friend class VideoRendererImplTest;
  friend class WallClockTimeSourceTest;

  // Construct a time source with an alternate clock for testing.
  explicit WallClockTimeSource(const base::TickClock* tick_clock);

  base::TimeDelta CurrentMediaTime_Locked() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  const raw_ptr<const base::TickClock> tick_clock_;

  bool ticking_ GUARDED_BY(lock_) = false;

  // While ticking we can interpolate the current media time by measuring the
  // delta between our reference ticks and the current system ticks and scaling
  // that time by the playback rate.
  double playback_rate_ GUARDED_BY(lock_) = 1.0;
  base::TimeDelta base_timestamp_ GUARDED_BY(lock_);
  base::TimeTicks reference_time_ GUARDED_BY(lock_);

  // TODO(scherkus): Remove internal locking from this class after access to
  // Renderer::CurrentMediaTime() is single threaded http://crbug.com/370634
  base::Lock lock_;
};

}  // namespace media

#endif  // MEDIA_BASE_WALL_CLOCK_TIME_SOURCE_H_
