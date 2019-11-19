// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TIMEOUT_MONITOR_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TIMEOUT_MONITOR_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"

namespace content {

// Utility class for handling a timeout callback with periodic starts and stops.
class CONTENT_EXPORT TimeoutMonitor {
 public:
  typedef base::RepeatingClosure TimeoutHandler;

  explicit TimeoutMonitor(const TimeoutHandler& timeout_handler);
  ~TimeoutMonitor();

  // Schedule the timeout timer to fire at |delay| into the future. If a timeout
  // has already been scheduled, reschedule only if |delay| is sooner than the
  // currently scheduled timeout time.
  void Start(base::TimeDelta delay);

  void Restart(base::TimeDelta delay);
  void Stop();
  bool IsRunning() const;

  base::TimeDelta GetCurrentDelay();

 private:
  void StartImpl(base::TimeDelta delay);
  void CheckTimedOut();

  TimeoutHandler timeout_handler_;

  // Indicates a time in the future when we would consider the input as
  // having timed out, if it does not receive an appropriate stop request.
  base::TimeTicks time_when_considered_timed_out_;

  // This timer runs to check if |time_when_considered_timed_out_| has past.
  base::OneShotTimer timeout_timer_;

  DISALLOW_COPY_AND_ASSIGN(TimeoutMonitor);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TIMEOUT_MONITOR_H_
