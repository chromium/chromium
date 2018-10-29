// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_BROWSER_TASK_EXECUTOR_H_
#define CONTENT_BROWSER_SCHEDULER_BROWSER_TASK_EXECUTOR_H_

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_executor.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_task_traits.h"

namespace content {

// This class's job is to map base::TaskTraits to actual task queues for the
// browser process.
class CONTENT_EXPORT BrowserTaskExecutor : public base::TaskExecutor {
 public:
  // Creates and registers a BrowserTaskExecutor that facilitates posting tasks
  // to a BrowserThread via //base/task/post_task.h.
  static void Create();

  // Unregister and delete the TaskExecutor after a test.
  static void ResetForTesting();

  // base::TaskExecutor implementation.
  bool PostDelayedTaskWithTraits(const base::Location& from_here,
                                 const base::TaskTraits& traits,
                                 base::OnceClosure task,
                                 base::TimeDelta delay) override;

  scoped_refptr<base::TaskRunner> CreateTaskRunnerWithTraits(
      const base::TaskTraits& traits) override;

  scoped_refptr<base::SequencedTaskRunner> CreateSequencedTaskRunnerWithTraits(
      const base::TaskTraits& traits) override;

  scoped_refptr<base::SingleThreadTaskRunner>
  CreateSingleThreadTaskRunnerWithTraits(
      const base::TaskTraits& traits,
      base::SingleThreadTaskRunnerThreadMode thread_mode) override;

#if defined(OS_WIN)
  scoped_refptr<base::SingleThreadTaskRunner> CreateCOMSTATaskRunnerWithTraits(
      const base::TaskTraits& traits,
      base::SingleThreadTaskRunnerThreadMode thread_mode) override;
#endif  // defined(OS_WIN)

 private:
  // For GetProxyTaskRunnerForThread().
  FRIEND_TEST_ALL_PREFIXES(BrowserTaskExecutorTest,
                           EnsureUIThreadTraitPointsToExpectedQueue);
  FRIEND_TEST_ALL_PREFIXES(BrowserTaskExecutorTest,
                           EnsureIOThreadTraitPointsToExpectedQueue);
  FRIEND_TEST_ALL_PREFIXES(BrowserTaskExecutorTest,
                           BestEffortTasksRunAfterStartup);

  BrowserTaskExecutor();
  ~BrowserTaskExecutor() override;

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      const base::TaskTraits& traits);

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      const base::TaskTraits& traits,
      const BrowserTaskTraitsExtension& extension);

  static scoped_refptr<base::SingleThreadTaskRunner>
  GetProxyTaskRunnerForThread(BrowserThread::ID id);

  static scoped_refptr<base::SingleThreadTaskRunner>
  GetAfterStartupTaskRunnerForThread(BrowserThread::ID id);

  DISALLOW_COPY_AND_ASSIGN(BrowserTaskExecutor);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCHEDULER_BROWSER_TASK_EXECUTOR_H_
