// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_ONE_SHOT_TIMEOUT_MONITOR_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_ONE_SHOT_TIMEOUT_MONITOR_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"

namespace content {

// Utility class for handling a timeout callback that can only be used once.
// This is effectively a wrapper for base::OneShotTimer that allows use of a
// base::OnceClosure.
class CONTENT_EXPORT OneShotTimeoutMonitor {
 public:
  typedef base::OnceClosure TimeoutHandler;

  // The timer starts upon construction.
  explicit OneShotTimeoutMonitor(TimeoutHandler timeout_handler,
                                 base::TimeDelta delay);

  OneShotTimeoutMonitor(const OneShotTimeoutMonitor&) = delete;
  OneShotTimeoutMonitor& operator=(const OneShotTimeoutMonitor&) = delete;

  ~OneShotTimeoutMonitor();

 private:
  void Start();
  void TimedOut();

  TimeoutHandler timeout_handler_;
  base::TimeDelta delay_;

  // This timer runs to check if |time_when_considered_timed_out_| has past.
  base::OneShotTimer timeout_timer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_ONE_SHOT_TIMEOUT_MONITOR_H_
