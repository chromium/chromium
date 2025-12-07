// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STARTUP_TASK_RUNNER_H_
#define CONTENT_BROWSER_STARTUP_TASK_RUNNER_H_

#include <list>

#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_main_runner.h"

namespace content {

// A startup task is a void function returning the status on completion.
// a status of > 0 indicates a failure, and that no further startup tasks should
// be run.
using StartupTask = base::OnceCallback<int(void)>;

// This class runs startup tasks. The tasks are either run immediately inline,
// or are queued one at a time on the UI thread's message loop. If the events
// are queued, UI events that are received during startup will be acted upon
// between startup tasks. The motivation for this is that, on targets where the
// UI is already started, it allows us to keep the UI responsive during startup.
//
// Note that this differs from a SingleThreadedTaskRunner in that there may be
// no opportunity to handle UI events between the tasks of a
// SingleThreadedTaskRunner.

class CONTENT_EXPORT StartupTaskRunner {

 public:
  // Constructor: Note that |startup_complete_callback| is optional. If it is
  // not null it will be called, once all the startup tasks have run, with the
  // result of running tasks and the duration spent blocking the UI thread.
  StartupTaskRunner(
      base::OnceCallback<void(int, base::TimeDelta, base::TimeDelta)>
          startup_complete_callback,
      scoped_refptr<base::SingleThreadTaskRunner> proxy);

  StartupTaskRunner(const StartupTaskRunner&) = delete;
  StartupTaskRunner& operator=(const StartupTaskRunner&) = delete;

  ~StartupTaskRunner();

  // Add a task to the queue of startup tasks to be run.
  void AddTask(StartupTask callback);

  // Start running the tasks asynchronously.
  void StartRunningTasksAsync();

  // Run all tasks, or all remaining tasks, synchronously
  // `was_posted` means the caller had posted this task to the task_runner
  // instead of executing it inline.
  void RunAllTasksNow(bool was_posted);

 private:
  friend class base::RefCounted<StartupTaskRunner>;

  std::list<StartupTask> task_list_;
  void WrappedTask();

  base::OnceCallback<void(int, base::TimeDelta, base::TimeDelta)>
      startup_complete_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> proxy_;
  // The longest of the wall-clock durations of each individual task that was
  // posted to the task_runner.
  base::TimeDelta longest_duration_of_posted_startup_tasks_;
  // The total wall-clock duration of all the tasks that were run as posted
  // tasks.
  base::TimeDelta total_duration_of_posted_startup_tasks_;
};

}  // namespace content
#endif  // CONTENT_BROWSER_STARTUP_TASK_RUNNER_H_
