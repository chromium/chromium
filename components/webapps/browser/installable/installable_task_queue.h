// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_TASK_QUEUE_H_
#define COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_TASK_QUEUE_H_

#include <deque>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_params.h"
#include "components/webapps/browser/installable/installable_task.h"

namespace webapps {

// InstallableTaskQueue keeps track of pending tasks.
class InstallableTaskQueue {
 public:
  InstallableTaskQueue();
  ~InstallableTaskQueue();

  // Adds task to the end of the active list of tasks to be processed.
  void Add(std::unique_ptr<InstallableTask> task);

  // Reports whether there are any tasks in the main list.
  bool HasCurrent() const;

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

  // The list of <params, callback> pairs that have come from a call to
  // InstallableManager::GetData.
  std::deque<std::unique_ptr<InstallableTask>> tasks_;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_TASK_QUEUE_H_
