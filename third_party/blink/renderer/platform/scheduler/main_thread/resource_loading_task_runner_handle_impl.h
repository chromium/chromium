// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_RESOURCE_LOADING_TASK_RUNNER_HANDLE_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_RESOURCE_LOADING_TASK_RUNNER_HANDLE_IMPL_H_

#include <memory>

#include "third_party/blink/public/platform/scheduler/web_resource_loading_task_runner_handle.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"

namespace blink {
namespace scheduler {

// Provides an interface to the loading stack (i.e. WebURLLoader) to post
// resource loading tasks and to notify blink's scheduler when a resource's
// fetch priority changes.
class PLATFORM_EXPORT ResourceLoadingTaskRunnerHandleImpl
    : public WebResourceLoadingTaskRunnerHandle {
 public:
  static std::unique_ptr<ResourceLoadingTaskRunnerHandleImpl> WrapTaskRunner(
      scoped_refptr<MainThreadTaskQueue> task_runner);

  ResourceLoadingTaskRunnerHandleImpl(
      const ResourceLoadingTaskRunnerHandleImpl&) = delete;
  ResourceLoadingTaskRunnerHandleImpl& operator=(
      const ResourceLoadingTaskRunnerHandleImpl&) = delete;

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() const override;

  void DidChangeRequestPriority(net::RequestPriority priority) override;

  const scoped_refptr<MainThreadTaskQueue>& task_queue();

  ~ResourceLoadingTaskRunnerHandleImpl() override;

 protected:
  explicit ResourceLoadingTaskRunnerHandleImpl(
      scoped_refptr<MainThreadTaskQueue> task_queue);

 private:
  scoped_refptr<MainThreadTaskQueue> task_queue_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_RESOURCE_LOADING_TASK_RUNNER_HANDLE_IMPL_H_
