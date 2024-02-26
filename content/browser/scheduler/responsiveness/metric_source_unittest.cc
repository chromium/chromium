// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/metric_source.h"

#include <atomic>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "content/browser/scheduler/responsiveness/native_event_observer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace responsiveness {
namespace {

class FakeDelegate : public MetricSource::Delegate {
 public:
  FakeDelegate()
      : set_up_on_io_thread_(false),
        tear_down_on_io_thread_(false),
        will_run_task_on_io_thread_(0),
        did_run_task_on_io_thread_(0) {}
  ~FakeDelegate() override = default;

  void SetUpOnIOThread() override { set_up_on_io_thread_ = true; }
  void TearDownOnUIThread() override { tear_down_on_ui_thread_ = true; }
  void TearDownOnIOThread() override { tear_down_on_io_thread_ = true; }

  void WillRunTaskOnUIThread(const base::PendingTask* task,
                             bool /* was_blocked_or_low_priority */) override {
    will_run_task_on_ui_thread_++;
  }
  void DidRunTaskOnUIThread(const base::PendingTask* task) override {
    did_run_task_on_ui_thread_++;
  }

  void WillRunTaskOnIOThread(const base::PendingTask* task,
                             bool /* was_blocked_or_low_priority */) override {
    will_run_task_on_io_thread_++;
  }
  void DidRunTaskOnIOThread(const base::PendingTask* task) override {
    did_run_task_on_io_thread_++;
  }

  void WillRunEventOnUIThread(const void* opaque_identifier) override {}
  void DidRunEventOnUIThread(const void* opaque_identifier) override {}

  bool set_up_on_io_thread() { return set_up_on_io_thread_; }
  bool tear_down_on_ui_thread() { return tear_down_on_ui_thread_; }
  bool tear_down_on_io_thread() { return tear_down_on_io_thread_; }

  int will_run_task_on_ui_thread() { return will_run_task_on_ui_thread_; }
  int did_run_task_on_ui_thread() { return did_run_task_on_ui_thread_; }
  int will_run_task_on_io_thread() { return will_run_task_on_io_thread_; }
  int did_run_task_on_io_thread() { return did_run_task_on_io_thread_; }

 private:
  std::atomic_bool set_up_on_io_thread_;
  bool tear_down_on_ui_thread_ = false;
  std::atomic_bool tear_down_on_io_thread_;

  int will_run_task_on_ui_thread_ = 0;
  int did_run_task_on_ui_thread_ = 0;

  std::atomic_int will_run_task_on_io_thread_;
  std::atomic_int did_run_task_on_io_thread_;
};

class TestMetricSource : public MetricSource {
 public:
  TestMetricSource(MetricSource::Delegate* delegate,
                   base::OnceClosure on_destroyed = base::DoNothing())
      : MetricSource(delegate), on_destroyed_(std::move(on_destroyed)) {}

  std::unique_ptr<NativeEventObserver> CreateNativeEventObserver() override {
    return nullptr;
  }

  ~TestMetricSource() override {
    DCHECK(on_destroyed_);
    std::move(on_destroyed_).Run();
  }

 private:
  base::OnceClosure on_destroyed_;
};

}  // namespace

class ResponsivenessMetricSourceTest : public testing::Test {
 public:
  ResponsivenessMetricSourceTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI,
                          content::BrowserTaskEnvironment::REAL_IO_THREAD) {}

  void SetUp() override { task_environment_.RunIOThreadUntilIdle(); }

  void TearDown() override {
    // Destroy a task onto the IO thread, which posts back to the UI thread
    // to complete destruction.
    task_environment_.RunIOThreadUntilIdle();
    task_environment_.RunUntilIdle();
  }

 protected:
  // This member sets up BrowserThread::IO and BrowserThread::UI. It must be the
  // first member, as other members may depend on these abstractions.
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ResponsivenessMetricSourceTest, SetUpTearDown) {
  std::unique_ptr<FakeDelegate> delegate = std::make_unique<FakeDelegate>();
  std::unique_ptr<TestMetricSource> metric_source =
      std::make_unique<TestMetricSource>(delegate.get());

  EXPECT_FALSE(delegate->set_up_on_io_thread());
  EXPECT_FALSE(delegate->tear_down_on_ui_thread());
  EXPECT_FALSE(delegate->tear_down_on_io_thread());

  metric_source->SetUp();
  // Test SetUpOnIOThread() is called after running the IO thread RunLoop.
  task_environment_.RunIOThreadUntilIdle();
  EXPECT_TRUE(delegate->set_up_on_io_thread());

  task_environment_.RunUntilIdle();

  base::ScopedClosureRunner on_finish_destroy(
      base::BindLambdaForTesting([&]() { metric_source = nullptr; }));
  metric_source->Destroy(std::move(on_finish_destroy));

  // Run IO thread to test TearDownOnIOThread().
  task_environment_.RunIOThreadUntilIdle();
  EXPECT_TRUE(delegate->tear_down_on_io_thread());
  EXPECT_FALSE(delegate->tear_down_on_ui_thread());

  // Run the UI thread to test TearDownOnUIThread().
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(delegate->tear_down_on_ui_thread());

  EXPECT_FALSE(metric_source);
}

// Test that the task callbacks are correctly called on UI and IO threads.
TEST_F(ResponsivenessMetricSourceTest, RunTasks) {
  std::unique_ptr<FakeDelegate> delegate = std::make_unique<FakeDelegate>();
  std::unique_ptr<TestMetricSource> metric_source =
      std::make_unique<TestMetricSource>(delegate.get());

  metric_source->SetUp();
  task_environment_.RunIOThreadUntilIdle();
  task_environment_.RunUntilIdle();

  GetUIThreadTaskRunner({})->PostTask(FROM_HERE, base::DoNothing());
  task_environment_.RunUntilIdle();
  EXPECT_GT(delegate->will_run_task_on_ui_thread(), 0);
  EXPECT_GT(delegate->did_run_task_on_ui_thread(), 0);

  content::GetIOThreadTaskRunner({})->PostTask(FROM_HERE, base::DoNothing());
  task_environment_.RunUntilIdle();
  EXPECT_GT(delegate->will_run_task_on_io_thread(), 0);
  EXPECT_GT(delegate->did_run_task_on_io_thread(), 0);

  base::ScopedClosureRunner on_finish_destroy(
      base::BindLambdaForTesting([&]() { metric_source = nullptr; }));
  metric_source->Destroy(std::move(on_finish_destroy));
  task_environment_.RunIOThreadUntilIdle();
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(metric_source);
}

}  // namespace responsiveness
}  // namespace content
