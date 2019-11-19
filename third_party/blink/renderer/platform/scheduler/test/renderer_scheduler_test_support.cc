// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"

#include <memory>

#include "base/single_thread_task_runner.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/blink/public/platform/scheduler/test/web_mock_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/dummy_schedulers.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {
namespace scheduler {

namespace {

class SimpleMockMainThreadScheduler : public WebMockThreadScheduler {
 public:
  SimpleMockMainThreadScheduler()
      : simple_thread_scheduler_(CreateDummyWebThreadScheduler()) {}
  ~SimpleMockMainThreadScheduler() override {}

  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override {
    return base::ThreadTaskRunnerHandle::Get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> InputTaskRunner() override {
    return base::ThreadTaskRunnerHandle::Get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> CleanupTaskRunner() override {
    return base::ThreadTaskRunnerHandle::Get();
  }

  std::unique_ptr<Thread> CreateMainThread() override {
    return simple_thread_scheduler_->CreateMainThread();
  }

 private:
  std::unique_ptr<WebThreadScheduler> simple_thread_scheduler_;
};

}  // namespace

std::unique_ptr<WebThreadScheduler> CreateWebMainThreadSchedulerForTests() {
  return CreateDummyWebThreadScheduler();
}

std::unique_ptr<WebMockThreadScheduler>
CreateMockWebMainThreadSchedulerForTests() {
  return std::make_unique<SimpleMockMainThreadScheduler>();
}

void RunIdleTasksForTesting(WebThreadScheduler* scheduler,
                            base::OnceClosure callback) {
  MainThreadSchedulerImpl* scheduler_impl =
      static_cast<MainThreadSchedulerImpl*>(scheduler);
  scheduler_impl->RunIdleTasksForTesting(std::move(callback));
}

scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunnerForTesting() {
  return base::SequencedTaskRunnerHandle::Get();
}

scoped_refptr<base::SingleThreadTaskRunner>
GetSingleThreadTaskRunnerForTesting() {
  return base::ThreadTaskRunnerHandle::Get();
}

}  // namespace scheduler
}  // namespace blink
