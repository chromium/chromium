#include "third_party/blink/renderer/platform/scheduler/main_thread/resource_loading_task_runner_handle_impl.h"

#include <iostream>

#include "base/memory/ptr_util.h"
#include "base/task/sequence_manager/task_queue.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"

namespace blink {
namespace scheduler {

using base::sequence_manager::TaskQueue;

std::unique_ptr<ResourceLoadingTaskRunnerHandleImpl>
ResourceLoadingTaskRunnerHandleImpl::WrapTaskRunner(
    scoped_refptr<MainThreadTaskQueue> task_queue) {
  DCHECK(task_queue);
  return base::WrapUnique(
      new ResourceLoadingTaskRunnerHandleImpl(std::move(task_queue)));
}

ResourceLoadingTaskRunnerHandleImpl::ResourceLoadingTaskRunnerHandleImpl(
    scoped_refptr<MainThreadTaskQueue> task_queue)
    : task_queue_(std::move(task_queue)),
      task_runner_(task_queue_->CreateTaskRunner(TaskType::kNetworking)) {}

ResourceLoadingTaskRunnerHandleImpl::~ResourceLoadingTaskRunnerHandleImpl() {
  if (task_queue_->GetFrameScheduler()) {
    task_queue_->GetFrameScheduler()->OnShutdownResourceLoadingTaskQueue(
        task_queue_);
  }
}

void ResourceLoadingTaskRunnerHandleImpl::DidChangeRequestPriority(
    net::RequestPriority priority) {
  FrameSchedulerImpl* frame_scheduler = task_queue_->GetFrameScheduler();
  if (frame_scheduler) {
    frame_scheduler->DidChangeResourceLoadingPriority(task_queue_, priority);
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
ResourceLoadingTaskRunnerHandleImpl::GetTaskRunner() const {
  return task_runner_;
}

const scoped_refptr<MainThreadTaskQueue>&
ResourceLoadingTaskRunnerHandleImpl::task_queue() {
  return task_queue_;
}

}  // namespace scheduler
}  // namespace blink
