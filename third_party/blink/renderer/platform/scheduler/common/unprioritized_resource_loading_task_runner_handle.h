// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_UNPRIORITIZED_RESOURCE_LOADING_TASK_RUNNER_HANDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_UNPRIORITIZED_RESOURCE_LOADING_TASK_RUNNER_HANDLE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/scheduler/web_resource_loading_task_runner_handle.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {
namespace scheduler {

// Provides a wrapper around task runners that do not support priorities.
// Intended to be used by WebURLLoader for posting tasks that are not bound to a
// frame.
class PLATFORM_EXPORT UnprioritizedResourceLoadingTaskRunnerHandle
    : public WebResourceLoadingTaskRunnerHandle {
 public:
  static std::unique_ptr<UnprioritizedResourceLoadingTaskRunnerHandle>
  WrapTaskRunner(scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  UnprioritizedResourceLoadingTaskRunnerHandle(
      const UnprioritizedResourceLoadingTaskRunnerHandle&) = delete;
  UnprioritizedResourceLoadingTaskRunnerHandle& operator=(
      const UnprioritizedResourceLoadingTaskRunnerHandle&) = delete;

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() const override;

  void DidChangeRequestPriority(net::RequestPriority priority) override;

  ~UnprioritizedResourceLoadingTaskRunnerHandle() override = default;

 protected:
  explicit UnprioritizedResourceLoadingTaskRunnerHandle(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_UNPRIORITIZED_RESOURCE_LOADING_TASK_RUNNER_HANDLE_H_
