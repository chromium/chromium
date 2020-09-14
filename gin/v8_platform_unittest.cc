// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/public/v8_platform.h"

#include <atomic>

#include "base/test/task_environment.h"
#include "base/trace_event/trace_event.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestTraceStateObserver
    : public v8::TracingController::TraceStateObserver {
 public:
  void OnTraceEnabled() final { ++enabled_; }
  void OnTraceDisabled() final { ++disabled_; }
  int Enabled() { return enabled_; }
  int Disabled() { return disabled_; }

 private:
  int enabled_ = 0;
  int disabled_ = 0;
};

namespace gin {

TEST(V8PlatformTest, TraceStateObserverAPI) {
  TestTraceStateObserver test_observer;
  ASSERT_EQ(0, test_observer.Enabled());
  ASSERT_EQ(0, test_observer.Disabled());

  V8Platform::Get()->GetTracingController()->AddTraceStateObserver(
      &test_observer);
  base::trace_event::TraceLog::GetInstance()->SetEnabled(
      base::trace_event::TraceConfig("*", ""),
      base::trace_event::TraceLog::RECORDING_MODE);
  ASSERT_EQ(1, test_observer.Enabled());
  ASSERT_EQ(0, test_observer.Disabled());
  base::trace_event::TraceLog::GetInstance()->SetDisabled();
  ASSERT_EQ(1, test_observer.Enabled());
  ASSERT_EQ(1, test_observer.Disabled());

  V8Platform::Get()->GetTracingController()->RemoveTraceStateObserver(
      &test_observer);
  base::trace_event::TraceLog::GetInstance()->SetEnabled(
      base::trace_event::TraceConfig("*", ""),
      base::trace_event::TraceLog::RECORDING_MODE);
  base::trace_event::TraceLog::GetInstance()->SetDisabled();
  ASSERT_EQ(1, test_observer.Enabled());
  ASSERT_EQ(1, test_observer.Disabled());
}

TEST(V8PlatformTest, TraceStateObserverFired) {
  TestTraceStateObserver test_observer;
  ASSERT_EQ(0, test_observer.Enabled());
  ASSERT_EQ(0, test_observer.Disabled());

  base::trace_event::TraceLog::GetInstance()->SetEnabled(
      base::trace_event::TraceConfig("*", ""),
      base::trace_event::TraceLog::RECORDING_MODE);
  V8Platform::Get()->GetTracingController()->AddTraceStateObserver(
      &test_observer);
  ASSERT_EQ(1, test_observer.Enabled());
  ASSERT_EQ(0, test_observer.Disabled());
}

// Tests that PostJob runs a task and is done after Join.
TEST(V8PlatformTest, PostJobSimple) {
  base::test::TaskEnvironment task_environment;
  std::atomic_size_t num_tasks_to_run(4);
  class Task : public v8::JobTask {
   public:
    explicit Task(std::atomic_size_t* num_tasks_to_run)
        : num_tasks_to_run(num_tasks_to_run) {}
    void Run(v8::JobDelegate* delegate) override { --(*num_tasks_to_run); }

    size_t GetMaxConcurrency(size_t /* worker_count*/) const override {
      return *num_tasks_to_run;
    }

    std::atomic_size_t* num_tasks_to_run;
  };
  auto handle =
      V8Platform::Get()->PostJob(v8::TaskPriority::kUserVisible,
                                 std::make_unique<Task>(&num_tasks_to_run));
  EXPECT_TRUE(handle->IsRunning());
  handle->Join();
  EXPECT_FALSE(handle->IsRunning());
  DCHECK_EQ(num_tasks_to_run, 0U);
}

}  // namespace gin
