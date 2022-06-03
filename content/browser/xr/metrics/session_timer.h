// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_XR_METRICS_SESSION_TIMER_H_
#define CONTENT_BROWSER_XR_METRICS_SESSION_TIMER_H_

#include "base/time/time.h"

namespace content {

// SessionTimer will monitor the time between calls to StartSession and
// StopSession.  It will combine multiple segments into a single session if they
// are sufficiently close in time.  It will also only include segments if they
// are sufficiently long.
// Because the session may be extended, the accumulated time is only sent when
// a StopSession call indicates that the session is no longer continuable, or
// on destruction.
class SessionTimer {
 public:
  SessionTimer(char const* histogram_name,
               base::TimeDelta gap_time,
               base::TimeDelta minimum_duration);

  virtual ~SessionTimer();

  SessionTimer(const SessionTimer&) = delete;
  SessionTimer& operator=(const SessionTimer&) = delete;

  void StartSession(base::Time start_time);
  void StopSession(bool continuable, base::Time stop_time);

 private:
  void SendAccumulatedSessionTime();

  char const* histogram_name_;

  base::Time start_time_;
  base::Time stop_time_;
  base::TimeDelta accumulated_time_;

  // Config members.
  // Maximum time gap allowed between a StopSession and a StartSession before it
  // will be logged as a separate session.
  base::TimeDelta maximum_session_gap_time_;

  // Minimum time between a StartSession and StopSession required before it is
  // added to the duration.
  base::TimeDelta minimum_duration_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_XR_METRICS_SESSION_TIMER_H_
