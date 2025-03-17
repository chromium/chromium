// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BTM_PERSISTENT_REPEATING_TIMER_H_
#define CONTENT_BROWSER_BTM_PERSISTENT_REPEATING_TIMER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"

namespace content {

// This class fires a task repeatedly, across application restarts. The timer
// stores the date of the last invocation in a preference, which is persisted
// to disk.
class CONTENT_EXPORT PersistentRepeatingTimer {
 public:
  class CONTENT_EXPORT Storage {
   public:
    using TimeCallback = base::OnceCallback<void(std::optional<base::Time>)>;
    virtual ~Storage();
    virtual void GetLastFired(TimeCallback callback) const = 0;
    virtual void SetLastFired(base::Time time) = 0;
  };

  // The timer is not started at creation.
  PersistentRepeatingTimer(std::unique_ptr<Storage> timer_storage,
                           base::TimeDelta delay,
                           base::RepeatingClosure task);

  ~PersistentRepeatingTimer();

  // Starts the timer. Calling Start() when the timer is running has no effect.
  void Start();

 private:
  // Called when |timer_| fires.
  void OnTimerFired();

  void StartWithLastFired(std::optional<base::Time> last_fired);

  std::unique_ptr<Storage> storage_;
  base::TimeDelta delay_;
  base::RepeatingClosure user_task_;

  base::RetainingOneShotTimer timer_;
  base::WeakPtrFactory<PersistentRepeatingTimer> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_BTM_PERSISTENT_REPEATING_TIMER_H_
