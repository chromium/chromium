// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_TASK_QUEUE_H_
#define COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_TASK_QUEUE_H_

#include <deque>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_params.h"

namespace webapps {

struct InstallableTask {
  InstallableTask();
  InstallableTask(const InstallableParams& params,
                  InstallableCallback callback);

  InstallableTask(const InstallableTask&) = delete;
  InstallableTask& operator=(const InstallableTask&) = delete;

  InstallableTask(InstallableTask&& other);
  InstallableTask& operator=(InstallableTask&& other);

  ~InstallableTask();

  InstallableParams params;
  InstallableCallback callback;
};

// InstallableTaskQueue keeps track of pending tasks.
class InstallableTaskQueue {
 public:
  InstallableTaskQueue();
  ~InstallableTaskQueue();

  // Adds task to the end of the active list of tasks to be processed.
  void Add(InstallableTask task);

  // Moves the current task from the main to the paused list.
  void PauseCurrent();

  // Moves all paused tasks to the main list.
  void UnpauseAll();

  // Reports whether there are any tasks in the main list.
  bool HasCurrent() const;

  // Reports whether there are any tasks in the paused list.
  bool HasPaused() const;

  // Returns the currently active task.
  InstallableTask& Current();

  // Advances to the next task.
  void Next();

  // Clears all tasks from the main and paused list, and then calls the callback
  // on all of them with the given status code.
  void ResetWithError(InstallableStatusCode code);

 private:
  friend class InstallableManagerBrowserTest;
  friend class InstallableManagerOfflineCapabilityBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest,
                           CheckLazyServiceWorkerPassesWhenWaiting);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest,
                           CheckLazyServiceWorkerNoFetchHandlerFails);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerOfflineCapabilityBrowserTest,
                           CheckLazyServiceWorkerPassesWhenWaiting);

  // The list of <params, callback> pairs that have come from a call to
  // InstallableManager::GetData.
  std::deque<InstallableTask> tasks_;

  // Tasks which are waiting indefinitely for a service worker to be detected.
  std::deque<InstallableTask> paused_tasks_;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_TASK_QUEUE_H_
