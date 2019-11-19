// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_POWER_OBSERVER_HELPER_H_
#define MEDIA_AUDIO_POWER_OBSERVER_HELPER_H_

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/power_monitor/power_observer.h"
#include "base/sequenced_task_runner.h"
#include "media/base/media_export.h"

namespace media {

// Helper class that implements PowerObserver and handles threading. A task
// runner is given, on which suspend and resume notification callbacks are run.
// It also provides a function to check if we are suspending on the task runner.
// Note that on Linux suspend/resume information is not supported.
class MEDIA_EXPORT PowerObserverHelper : public base::PowerObserver {
 public:
  PowerObserverHelper(scoped_refptr<base::SequencedTaskRunner> task_runner,
                      base::RepeatingClosure suspend_callback,
                      base::RepeatingClosure resume_callback);

  ~PowerObserverHelper() override;

  // Must be called on |task_runner|.
  virtual bool IsSuspending() const;

 protected:
  base::SequencedTaskRunner* TaskRunnerForTesting() const {
    return task_runner_.get();
  }

  base::RepeatingClosure* SuspendCallbackForTesting() {
    return &suspend_callback_;
  }

  base::RepeatingClosure* ResumeCallbackForTesting() {
    return &resume_callback_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(PowerObserverHelperTest,
                           SuspendAndResumeNotificationsTwice);
  FRIEND_TEST_ALL_PREFIXES(PowerObserverHelperTest,
                           TwoSuspendAndTwoResumeNotifications);

  // The task runner on which |is_suspending_| should live and the callbacks
  // should be run on.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Suspend and resume callbacks. Run on |task_runner_|.
  base::RepeatingClosure suspend_callback_;
  base::RepeatingClosure resume_callback_;

  // base::PowerObserver implementation.
  void OnSuspend() override;
  void OnResume() override;

  // Flag if we are suspending.
  bool is_suspending_ = false;

  base::WeakPtrFactory<PowerObserverHelper> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PowerObserverHelper);
};

}  // namespace media

#endif  // MEDIA_AUDIO_POWER_OBSERVER_HELPER_H_
