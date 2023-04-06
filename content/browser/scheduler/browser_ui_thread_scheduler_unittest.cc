// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_ui_thread_scheduler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_features.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace features {
BASE_FEATURE(kBrowserDeferUIThreadTasks,
             "BrowserDeferUIThreadTasks",
             base::FEATURE_DISABLED_BY_DEFAULT);
}

class BrowserUIThreadSchedulerTest : public testing::Test {
 protected:
  BrowserUIThreadSchedulerTest() = default;

  void EnablePostFeatureListSetup() {
    browser_ui_thread_scheduler_->Get()->PostFeatureListSetup();
  }

  void SetUp() override {
    browser_ui_thread_scheduler_ = std::make_unique<BrowserUIThreadScheduler>();
    browser_ui_thread_scheduler_->GetHandle()->OnStartupComplete();
  }

  void SetDeferringExperimentEnabled() {
    auto defer_browser_ui_tasks = base::test::FeatureRefAndParams(
        features::kBrowserDeferUIThreadTasks,
        {{"defer_normal_or_less_priority_tasks", "true"},
         {"defer_known_long_running_tasks", "true"}});
    feature_list_.InitWithFeaturesAndParameters({defer_browser_ui_tasks}, {});
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler_;
};

// namespace features

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

TEST_F(BrowserUIThreadSchedulerTest, DestructorPostChainDuringShutdown) {
  auto task_queue =
      browser_ui_thread_scheduler_->GetHandle()->GetBrowserTaskRunner(
          BrowserUIThreadScheduler::QueueType::kUserBlocking);

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

TEST_F(BrowserUIThreadSchedulerTest,
       TaskPostedWithThreadHandleRunBeforeQueuesAreEnabled) {
  StrictMockTask task;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                              task.Get());

  EXPECT_CALL(task, Run);
  base::RunLoop().RunUntilIdle();
}

TEST_F(BrowserUIThreadSchedulerTest, TestPostedTasksDeferredDuringScroll) {
  // Setup
  SetDeferringExperimentEnabled();
  bool did_run_task = false;

  // Loop has to run after setting up as experiment is enabled after
  // startup via post task.
  EnablePostFeatureListSetup();
  base::RunLoop().RunUntilIdle();

  // Post a user blocking task and pump the RunLoop.
  browser_ui_thread_scheduler_->OnScrollStateUpdate(
      BrowserUIThreadScheduler::ScrollState::kFlingActive);
  browser_ui_thread_scheduler_->GetHandle()
      ->GetBrowserTaskRunner(BrowserTaskQueues::QueueType::kUserBlocking)
      ->PostTask(
          FROM_HERE,
          base::BindOnce([](bool* did_run_task) { *did_run_task = true; },
                         &did_run_task));
  base::RunLoop().RunUntilIdle();

  // Assert that task didn't run during a scroll.
  EXPECT_FALSE(did_run_task);
  browser_ui_thread_scheduler_->OnScrollStateUpdate(
      BrowserUIThreadScheduler::ScrollState::kNone);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(did_run_task);
}

TEST_F(BrowserUIThreadSchedulerTest, TestPostedTasksNotDeferredWithoutScroll) {
  // Setup
  SetDeferringExperimentEnabled();
  bool did_run_task = false;

  // Loop has to run after setting up as experiment is enabled after
  // startup via post task.
  EnablePostFeatureListSetup();
  base::RunLoop().RunUntilIdle();

  // Post a user blocking task and pump the RunLoop.
  browser_ui_thread_scheduler_->GetHandle()
      ->GetBrowserTaskRunner(BrowserTaskQueues::QueueType::kUserBlocking)
      ->PostTask(
          FROM_HERE,
          base::BindOnce([](bool* did_run_task) { *did_run_task = true; },
                         &did_run_task));
  base::RunLoop().RunUntilIdle();

  // Assert that task was run as there is no scroll.
  EXPECT_TRUE(did_run_task);
}

TEST_F(BrowserUIThreadSchedulerTest,
       TestPostedTasksNotDeferredDuringScrollExperimentDisabled) {
  // Setup
  bool did_run_task = false;

  // Loop has to run after setting up as experiment is enabled after
  // startup via post task.
  EnablePostFeatureListSetup();
  base::RunLoop().RunUntilIdle();

  // Post a user blocking task and pump the RunLoop.
  browser_ui_thread_scheduler_->OnScrollStateUpdate(
      BrowserUIThreadScheduler::ScrollState::kFlingActive);
  browser_ui_thread_scheduler_->GetHandle()
      ->GetBrowserTaskRunner(BrowserTaskQueues::QueueType::kUserBlocking)
      ->PostTask(
          FROM_HERE,
          base::BindOnce([](bool* did_run_task) { *did_run_task = true; },
                         &did_run_task));
  base::RunLoop().RunUntilIdle();

  // Assert that task was run during scroll as experiment is disabled.
  EXPECT_TRUE(did_run_task);
}

}  // namespace

}  // namespace content
