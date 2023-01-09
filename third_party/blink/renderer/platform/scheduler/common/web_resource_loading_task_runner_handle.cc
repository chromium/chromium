#include "third_party/blink/public/platform/scheduler/web_resource_loading_task_runner_handle.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/scheduler/common/unprioritized_resource_loading_task_runner_handle.h"

namespace blink {
namespace scheduler {

std::unique_ptr<WebResourceLoadingTaskRunnerHandle>
WebResourceLoadingTaskRunnerHandle::CreateUnprioritized(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(task_runner);
  return UnprioritizedResourceLoadingTaskRunnerHandle::WrapTaskRunner(
      std::move(task_runner));
}

}  // namespace scheduler
}  // namespace blink
