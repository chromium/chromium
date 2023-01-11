// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SIMPLE_WATCH_TIMER_H_
#define MEDIA_BASE_SIMPLE_WATCH_TIMER_H_

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/base/media_export.h"

namespace media {

// SimpleWatchTimer aids in recording UMA counts that accumulate media watch
// time in seconds. It will fire its callback about once per second during
// active playback.
//
// Active playback is a duration after Start() and before Stop() in
// which current time progresses. Large jumps in current time are not considered
// to be progress; they are assumed to be seeks or media errors.
//
// Start() and Stop() may be called repeatedly. It is recommended to call Stop()
// before destructing a SimpleWatchTimer so that |tick_cb| can be fired at an
// opportune time.
//
// Note: SimpleWatchTimer does not understand playbackRate and will discard
// durations with high rates.
class MEDIA_EXPORT SimpleWatchTimer {
 public:
  using TickCB = base::RepeatingClosure;
  using GetCurrentTimeCB = base::RepeatingCallback<base::TimeDelta()>;

  SimpleWatchTimer(TickCB tick_cb, GetCurrentTimeCB get_current_time_cb);

  SimpleWatchTimer(const SimpleWatchTimer&) = delete;
  SimpleWatchTimer& operator=(const SimpleWatchTimer&) = delete;

  ~SimpleWatchTimer();

  void Start();
  void Stop();

 private:
  void Tick();

  TickCB tick_cb_;
  GetCurrentTimeCB get_current_time_cb_;

  int unreported_ms_ = 0;
  base::TimeDelta last_current_time_;
  base::RepeatingTimer timer_;
};

}  // namespace media

#endif  // MEDIA_BASE_SIMPLE_WATCH_TIMER_H_
