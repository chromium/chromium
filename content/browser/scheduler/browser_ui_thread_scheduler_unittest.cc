// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_ui_thread_scheduler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/mock_callback.h"
#include "content/public/browser/browser_task_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using StrictMockTask =
    testing::StrictMock<base::MockCallback<base::RepeatingCallback<void()>>>;

base::OnceClosure RunOnDestruction(base::OnceClosure task) {
  return base::BindOnce(
      [](std::unique_ptr<base::ScopedClosureRunner>) {},
      std::make_unique<base::ScopedClosureRunner>(std::move(task)));
}

base::OnceClosure PostOnDestruction(
    scoped_refptr<base::SingleThreadTaskRunner> task_queue,
    base::OnceClosure task) {
  return RunOnDestruction(base::BindOnce(
      [](base::OnceClosure task,
         scoped_refptr<base::SingleThreadTaskRunner> task_queue) {
        task_queue->PostTask(FROM_HERE, std::move(task));
      },
      std::move(task), task_queue));
}

TEST(BrowserUIThreadSchedulerTest, DestructorPostChainDuringShutdown) {
  auto browser_ui_thread_scheduler_ =
      std::make_unique<BrowserUIThreadScheduler>();
  browser_ui_thread_scheduler_->GetHandle()->OnStartupComplete();
  auto task_queue =
      browser_ui_thread_scheduler_->GetHandle()->GetBrowserTaskRunner(
          BrowserUIThreadScheduler::QueueType::kDefault);

  bool run = false;
  task_queue->PostTask(
      FROM_HERE,
      PostOnDestruction(
          task_queue,
          PostOnDestruction(task_queue,
                            RunOnDestruction(base::BindOnce(
                                [](bool* run) { *run = true; }, &run)))));

  EXPECT_FALSE(run);
  browser_ui_thread_scheduler_.reset();

  EXPECT_TRUE(run);
}

TEST(BrowserUIThreadSchedulerTest,
     TaskPostedWithThreadHandleRunBeforeQueuesAreEnabled) {
  auto browser_ui_thread_scheduler_ =
      std::make_unique<BrowserUIThreadScheduler>();

  StrictMockTask task;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                              task.Get());

  EXPECT_CALL(task, Run);
  base::RunLoop().RunUntilIdle();
}

}  // namespace

}  // namespace content
