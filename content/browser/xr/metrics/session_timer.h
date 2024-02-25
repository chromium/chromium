// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_XR_METRICS_SESSION_TIMER_H_
#define CONTENT_BROWSER_XR_METRICS_SESSION_TIMER_H_

#include "base/time/time.h"

namespace content {

// SessionTimer will monitor the time between calls to StartSession and
// StopSession, and will send the duration upon either StopSession or
// destruction.
class SessionTimer {
 public:
  explicit SessionTimer(size_t trace_id);

  virtual ~SessionTimer();

  SessionTimer(const SessionTimer&) = delete;
  SessionTimer& operator=(const SessionTimer&) = delete;

  base::Time GetStartTime();
  size_t GetTraceId();
  void StartSession();
  void StopSession();

 private:
  base::Time start_time_;
  size_t trace_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_XR_METRICS_SESSION_TIMER_H_
