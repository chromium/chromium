// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_RETAINING_ONE_SHOT_TIMER_HOLDER_H_
#define GPU_COMMAND_BUFFER_SERVICE_RETAINING_ONE_SHOT_TIMER_HOLDER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "gpu/gpu_export.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gpu {

// This class holds a RetainingOneShotTimer and ensures thread safety.
// The public methods are safe to call from any threads.
// Before the object is destructed, DestroyTimer() must be called to clean up.
class GPU_EXPORT RetainingOneShotTimerHolder
    : public base::RefCountedThreadSafe<RetainingOneShotTimerHolder> {
 public:
  // `max_delay` and `min_delay` together specify how ResetTimerIfNecessary()
  // sets up the timer to fire. Please see comments of that method.
  //
  // `task_runner` is the runner on which the timer operates and destructs.
  // `user_task` is called on `task_runner` whenever the timer is fired.
  //
  // Note: To avoid race between `user_task` execution and clean up in
  // DestroyTimer(), `user_task` is run while holding `lock_`. Therefore:
  // - `user_task` must not call into DestroyTimer();
  // - if `user_task` holds certain locks, in order to avoid lock order
  //   inversion, callers must not call DestroyTimer() under those locks.
  RetainingOneShotTimerHolder(
      base::TimeDelta max_delay,
      base::TimeDelta min_delay,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::RepeatingClosure user_task);

  RetainingOneShotTimerHolder(const RetainingOneShotTimerHolder& other) =
      delete;
  RetainingOneShotTimerHolder& operator=(
      const RetainingOneShotTimerHolder& other) = delete;

  // If the timer is already running and the remaining time is between
  // `max_delay` and `min_delay`, then it is a no-op; otherwise, the timer is
  // reset to first after `max_delay`.
  void ResetTimerIfNecessary();

  // Destroys the timer. Once the method returns, it guarantees that
  // `user_task_` won't be called afterwards. This method will block if
  // `user_task_` is being called.
  //
  // This method must be called to clean up.
  void DestroyTimer() LOCKS_EXCLUDED(lock_);

 private:
  friend class base::RefCountedThreadSafe<RetainingOneShotTimerHolder>;
  ~RetainingOneShotTimerHolder();

  void DestroyTimerOnTaskRunner();
  void OnTimerFired() LOCKS_EXCLUDED(lock_);

  const base::TimeDelta min_delay_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  mutable base::Lock lock_;
  base::RepeatingClosure user_task_ GUARDED_BY(lock_);

  // Operates exclusively on `task_runner_`.
  std::unique_ptr<base::RetainingOneShotTimer> timer_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_RETAINING_ONE_SHOT_TIMER_HOLDER_H_
