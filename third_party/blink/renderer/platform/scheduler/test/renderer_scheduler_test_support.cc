// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"

#include <memory>

#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/scheduler/test/web_mock_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/dummy_schedulers.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {
namespace scheduler {

namespace {

class SimpleMockMainThreadScheduler : public WebMockThreadScheduler {
 public:
  SimpleMockMainThreadScheduler()
      : simple_thread_scheduler_(CreateDummyWebMainThreadScheduler()) {}
  ~SimpleMockMainThreadScheduler() override = default;

  std::unique_ptr<MainThread> CreateMainThread() override {
    return simple_thread_scheduler_->CreateMainThread();
  }

 private:
  std::unique_ptr<WebThreadScheduler> simple_thread_scheduler_;
};

}  // namespace

std::unique_ptr<WebThreadScheduler> CreateWebMainThreadSchedulerForTests() {
  return CreateDummyWebMainThreadScheduler();
}

std::unique_ptr<WebMockThreadScheduler>
CreateMockWebMainThreadSchedulerForTests() {
  return std::make_unique<SimpleMockMainThreadScheduler>();
}

scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunnerForTesting() {
  return base::SequencedTaskRunner::GetCurrentDefault();
}

scoped_refptr<base::SingleThreadTaskRunner>
GetSingleThreadTaskRunnerForTesting() {
  return base::SingleThreadTaskRunner::GetCurrentDefault();
}

}  // namespace scheduler
}  // namespace blink
