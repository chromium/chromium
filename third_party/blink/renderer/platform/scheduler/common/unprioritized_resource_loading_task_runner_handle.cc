#include "third_party/blink/renderer/platform/scheduler/common/unprioritized_resource_loading_task_runner_handle.h"

#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"

namespace blink {
namespace scheduler {

std::unique_ptr<UnprioritizedResourceLoadingTaskRunnerHandle>
UnprioritizedResourceLoadingTaskRunnerHandle::WrapTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(task_runner);
  return base::WrapUnique(
      new UnprioritizedResourceLoadingTaskRunnerHandle(std::move(task_runner)));
}

UnprioritizedResourceLoadingTaskRunnerHandle::
    UnprioritizedResourceLoadingTaskRunnerHandle(
        scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

scoped_refptr<base::SingleThreadTaskRunner>
UnprioritizedResourceLoadingTaskRunnerHandle::GetTaskRunner() const {
  return task_runner_;
}

void UnprioritizedResourceLoadingTaskRunnerHandle::DidChangeRequestPriority(
    net::RequestPriority priority) {}

}  // namespace scheduler
}  // namespace blink
