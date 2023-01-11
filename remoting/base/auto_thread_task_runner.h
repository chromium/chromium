// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_AUTO_THREAD_TASK_RUNNER_H_
#define REMOTING_BASE_AUTO_THREAD_TASK_RUNNER_H_

#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "build/buildflag.h"

namespace remoting {

// A wrapper around |SingleThreadTaskRunner| that provides automatic lifetime
// management, by posting a caller-supplied task to the underlying task runner
// when no more references remain.
class AutoThreadTaskRunner : public base::SingleThreadTaskRunner {
 public:
#if BUILDFLAG(IS_CHROMEOS)
  // Constructs an instance of |AutoThreadTaskRunner| wrapping |task_runner|.
  // This c'tor should be used when the underlying thread is not owned, such as
  // when this instance is running on a browser thread on ChromeOS.
  explicit AutoThreadTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
#endif

  // Constructs an instance of |AutoThreadTaskRunner| wrapping |task_runner|.
  // |stop_task| is posted to |task_runner| when the last reference to
  // the AutoThreadTaskRunner is dropped.
  AutoThreadTaskRunner(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                       base::OnceClosure stop_task);

  AutoThreadTaskRunner(const AutoThreadTaskRunner&) = delete;
  AutoThreadTaskRunner& operator=(const AutoThreadTaskRunner&) = delete;

  // SingleThreadTaskRunner implementation
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override;
  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override;
  bool RunsTasksInCurrentSequence() const override;

  const scoped_refptr<base::SingleThreadTaskRunner>& task_runner() {
    return task_runner_;
  }

 private:
  ~AutoThreadTaskRunner() override;

  // Task posted to |task_runner_| to notify the caller that it may be stopped.
  base::OnceClosure stop_task_;

  // The wrapped task runner.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_AUTO_THREAD_TASK_RUNNER_H_
