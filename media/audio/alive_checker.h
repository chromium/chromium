// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ALIVE_CHECKER_H_
#define MEDIA_AUDIO_ALIVE_CHECKER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "media/audio/power_observer_helper.h"
#include "media/base/media_export.h"

namespace media {

// A class that checks if a client that is expected to have regular activity
// is alive. For example, audio streams expect regular callbacks from the
// operating system. The client informs regularly that it's alive by calling
// NotifyAlive(). At a certain interval the AliveChecker checks that it has been
// notified within a timeout period. If not, it runs a callback to inform about
// detecting dead. The callback is run once and further checking is stopped at
// detection. Checking can be restarted if desired.
//
// The AliveChecker can pause checking when the machine is suspending, i.e.
// between suspend and resume notification from base::PowerMonitor. Checking
// during this period can cause false positives. Shorter timeout gives higher
// risk of false positives.
//
// It lives on the task runner it's created on; all functions except
// NotifyAlive() must be called on it. NotifyAlive() can be called on any task
// runner.
//
// It stops at the first NotifyAlive() call if
// |stop_at_first_alive_notification| is specified at construction time. This
// can be useful for example if the platform doesn't support suspend/resume
// notifications, as Linux.
class MEDIA_EXPORT AliveChecker {
 public:
  // Factory callback to create a PowerObserverHelper that can be injected. Can
  // be used by tests to provide a mock.
  using PowerObserverHelperFactoryCallback =
      base::OnceCallback<std::unique_ptr<PowerObserverHelper>(
          scoped_refptr<base::SequencedTaskRunner> task_runner,
          base::RepeatingClosure suspend_callback,
          base::RepeatingClosure resume_callback)>;

  // See class description for general explanation of parameters.
  // In addition the following must be true: |timeout| > |check_interval| > 0.
  // The first version creates a PowerObserverHelper internally, the second
  // version takes a factory callback to allow injecting a PowerObserverHelper,
  // typically a mock for testing. The callback is run in the constructor. The
  // second version doesn't have |pause_check_during_suspend|, since that's
  // implicitly true when providing a PowerObserverHelper.
  AliveChecker(base::RepeatingClosure dead_callback,
               base::TimeDelta check_interval,
               base::TimeDelta timeout,
               bool stop_at_first_alive_notification,
               bool pause_check_during_suspend);
  AliveChecker(base::RepeatingClosure dead_callback,
               base::TimeDelta check_interval,
               base::TimeDelta timeout,
               bool stop_at_first_alive_notification,
               PowerObserverHelperFactoryCallback
                   power_observer_helper_factory_callback);

  ~AliveChecker();

  // Start and stop checking if the client is alive.
  void Start();
  void Stop();

  // Returns whether dead was detected. Reset when Start() is called.
  bool DetectedDead();

  // Called regularly by the client to inform that it's alive. Can be called on
  // any thread.
  void NotifyAlive();

 private:
  // Internal version called by the public constructors, to keep the interface
  // and contract clear in the public versions.
  AliveChecker(base::RepeatingClosure dead_callback,
               base::TimeDelta check_interval,
               base::TimeDelta timeout,
               bool stop_at_first_alive_notification,
               bool pause_check_during_suspend,
               PowerObserverHelperFactoryCallback
                   power_observer_helper_factory_callback);

  // Checks if we have gotten an alive notification within a certain time
  // period. If not, run |dead_callback_|.
  void CheckIfAlive();

  // Sets |last_alive_notification_time_| to the current time.
  void SetLastAliveNotificationTimeToNowOnTaskRunner();

  // Timer to run the check regularly.
  std::unique_ptr<base::RepeatingTimer> check_alive_timer_;

  // Stores the time NotifyAlive() was last called.
  // TODO(grunell): Change from TimeTicks to Atomic32 and remove the task
  // posting in NotifyAlive(). The Atomic32 variable would have to
  // represent some time in seconds or tenths of seconds to be able to span over
  // enough time. Atomic64 cannot be used since it's not supported on 32-bit
  // platforms.
  base::TimeTicks last_alive_notification_time_;

  // The interval at which we check if alive.
  const base::TimeDelta check_interval_;

  // The time interval since |last_alive_notification_time_| after which we
  // decide the client is dead and run |dead_callback_|.
  const base::TimeDelta timeout_;

  // Flags that dead was detected. Set in CheckIfAlive() if we have decided that
  // the client is dead. Cleared in Start().
  bool detected_dead_ = false;

  // The task runner on which this object lives.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Dead notification callback.
  base::RepeatingClosure dead_callback_;

  // If true, checking stops after first alive notification, otherwise continues
  // until Stop() is called or the client is decided to be dead.
  const bool stop_at_first_alive_notification_;

  // Used for getting suspend/resume notifications.
  std::unique_ptr<PowerObserverHelper> power_observer_;

  base::WeakPtrFactory<AliveChecker> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AliveChecker);
};

}  // namespace media

#endif  // MEDIA_AUDIO_ALIVE_CHECKER_H_
