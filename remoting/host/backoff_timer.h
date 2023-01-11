// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_BACKOFF_TIMER_H_
#define REMOTING_HOST_BACKOFF_TIMER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/timer/timer.h"
#include "net/base/backoff_entry.h"

namespace remoting {

// An object similar to a base::Timer with exponential backoff.
class BackoffTimer {
 public:
  BackoffTimer();

  BackoffTimer(const BackoffTimer&) = delete;
  BackoffTimer& operator=(const BackoffTimer&) = delete;

  ~BackoffTimer();

  // Invokes |user_task| at intervals specified by |delay|, and
  // increasing up to |max_delay|.  Always invokes |user_task| before
  // the first scheduled delay.
  void Start(const base::Location& posted_from,
             base::TimeDelta delay,
             base::TimeDelta max_delay,
             const base::RepeatingClosure& user_task);

  // Prevents the user task from being invoked again.
  void Stop();

  // Returns true if the user task may be invoked in the future.
  bool IsRunning() const { return !!backoff_entry_; }

 private:
  void StartTimer();
  void OnTimerFired();

  base::OneShotTimer timer_;
  base::RepeatingClosure user_task_;
  base::Location posted_from_;
  net::BackoffEntry::Policy backoff_policy_ = {};
  std::unique_ptr<net::BackoffEntry> backoff_entry_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_BACKOFF_TIMER_H_
