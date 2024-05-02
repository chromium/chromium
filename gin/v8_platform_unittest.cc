// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/public/v8_platform.h"

#include <atomic>

#include "base/barrier_closure.h"
#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_waitable_event.h"
#include "base/trace_event/trace_event.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace gin {

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

    raw_ptr<std::atomic_size_t> num_tasks_to_run;
  };
  auto handle =
      V8Platform::Get()->PostJob(v8::TaskPriority::kUserVisible,
                                 std::make_unique<Task>(&num_tasks_to_run));
  EXPECT_TRUE(handle->IsValid());
  handle->Join();
  EXPECT_FALSE(handle->IsValid());
  DCHECK_EQ(num_tasks_to_run, 0U);
}

// Tests that JobTask's lifetime is extended beyond job handle, until no
// references are left; and is gracefully destroyed.
TEST(V8PlatformTest, PostJobLifetime) {
  std::atomic_size_t num_tasks_to_run(4);

  base::TestWaitableEvent threads_running;
  base::TestWaitableEvent threads_continue;
  base::RepeatingClosure threads_running_barrier = base::BarrierClosure(
      num_tasks_to_run,
      BindOnce(&base::TestWaitableEvent::Signal, Unretained(&threads_running)));

  class Task : public v8::JobTask {
   public:
    explicit Task(std::atomic_size_t* num_tasks_to_run,
                  base::RepeatingClosure threads_running_barrier,
                  base::TestWaitableEvent* threads_continue)
        : num_tasks_to_run_(num_tasks_to_run),
          threads_running_barrier_(std::move(threads_running_barrier)),
          threads_continue_(threads_continue) {}
    ~Task() override {
      // This should only be destroyed once all workers returned.
      EXPECT_EQ(*num_tasks_to_run_, 0U);
    }

    void Run(v8::JobDelegate* delegate) override {
      threads_running_barrier_.Run();
      threads_continue_->Wait();
      --(*num_tasks_to_run_);
    }

    size_t GetMaxConcurrency(size_t /* worker_count*/) const override {
      return *num_tasks_to_run_;
    }

    raw_ptr<std::atomic_size_t> num_tasks_to_run_;
    base::RepeatingClosure threads_running_barrier_;
    raw_ptr<base::TestWaitableEvent> threads_continue_;
  };

  base::test::TaskEnvironment task_environment;

  auto handle = V8Platform::Get()->PostJob(
      v8::TaskPriority::kUserVisible,
      std::make_unique<Task>(&num_tasks_to_run,
                             std::move(threads_running_barrier),
                             &threads_continue));
  EXPECT_TRUE(handle->IsValid());
  threads_running.Wait();
  handle->CancelAndDetach();
  handle.reset();

  // Release workers and let the job get destroyed.
  threads_continue.Signal();
}

}  // namespace gin
