// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/common/scoped_defer_task_posting.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/base_tracing.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/common/blink_scheduler_single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

namespace blink {
namespace scheduler {

namespace internal {
using base::sequence_manager::internal::TaskQueueImpl;
}

using perfetto::protos::pbzero::ChromeTrackEvent;
using perfetto::protos::pbzero::RendererMainThreadTaskExecution;
using QueueName = ::perfetto::protos::pbzero::SequenceManagerTask::QueueName;
// static
QueueName MainThreadTaskQueue::NameForQueueType(
    MainThreadTaskQueue::QueueType queue_type) {
  switch (queue_type) {
    case MainThreadTaskQueue::QueueType::kControl:
      return QueueName::CONTROL_TQ;
    case MainThreadTaskQueue::QueueType::kDefault:
      return QueueName::DEFAULT_TQ;
    case MainThreadTaskQueue::QueueType::kFrameLoading:
      return QueueName::FRAME_LOADING_TQ;
    case MainThreadTaskQueue::QueueType::kFrameThrottleable:
      return QueueName::FRAME_THROTTLEABLE_TQ;
    case MainThreadTaskQueue::QueueType::kFrameDeferrable:
      return QueueName::FRAME_DEFERRABLE_TQ;
    case MainThreadTaskQueue::QueueType::kFramePausable:
      return QueueName::FRAME_PAUSABLE_TQ;
    case MainThreadTaskQueue::QueueType::kFrameUnpausable:
      return QueueName::FRAME_UNPAUSABLE_TQ;
    case MainThreadTaskQueue::QueueType::kCompositor:
      return QueueName::COMPOSITOR_TQ;
    case MainThreadTaskQueue::QueueType::kIdle:
      return QueueName::IDLE_TQ;
    case MainThreadTaskQueue::QueueType::kTest:
      return QueueName::TEST_TQ;
    case MainThreadTaskQueue::QueueType::kFrameLoadingControl:
      return QueueName::FRAME_LOADING_CONTROL_TQ;
    case MainThreadTaskQueue::QueueType::kV8:
      return QueueName::V8_TQ;
    case MainThreadTaskQueue::QueueType::kV8UserVisible:
      return QueueName::V8_USER_VISIBLE_TQ;
    case MainThreadTaskQueue::QueueType::kV8BestEffort:
      return QueueName::V8_BEST_EFFORT_TQ;
    case MainThreadTaskQueue::QueueType::kInput:
      return QueueName::INPUT_TQ;
    case MainThreadTaskQueue::QueueType::kDetached:
      return QueueName::DETACHED_TQ;
    case MainThreadTaskQueue::QueueType::kOther:
      return QueueName::OTHER_TQ;
    case MainThreadTaskQueue::QueueType::kWebScheduling:
      return QueueName::WEB_SCHEDULING_TQ;
    case MainThreadTaskQueue::QueueType::kNonWaking:
      return QueueName::NON_WAKING_TQ;
    case MainThreadTaskQueue::QueueType::kIPCTrackingForCachedPages:
      return QueueName::IPC_TRACKING_FOR_CACHED_PAGES_TQ;
    case MainThreadTaskQueue::QueueType::kCount:
      NOTREACHED_IN_MIGRATION();
      return QueueName::UNKNOWN_TQ;
  }
  NOTREACHED_IN_MIGRATION();
  return QueueName::UNKNOWN_TQ;
}

// static
bool MainThreadTaskQueue::IsPerFrameTaskQueue(
    MainThreadTaskQueue::QueueType queue_type) {
  switch (queue_type) {
    // TODO(altimin): Remove kDefault once there is no per-frame kDefault queue.
    case MainThreadTaskQueue::QueueType::kDefault:
    case MainThreadTaskQueue::QueueType::kFrameLoading:
    case MainThreadTaskQueue::QueueType::kFrameLoadingControl:
    case MainThreadTaskQueue::QueueType::kFrameThrottleable:
    case MainThreadTaskQueue::QueueType::kFrameDeferrable:
    case MainThreadTaskQueue::QueueType::kFramePausable:
    case MainThreadTaskQueue::QueueType::kFrameUnpausable:
    case MainThreadTaskQueue::QueueType::kIdle:
    case MainThreadTaskQueue::QueueType::kWebScheduling:
      return true;
    case MainThreadTaskQueue::QueueType::kControl:
    case MainThreadTaskQueue::QueueType::kCompositor:
    case MainThreadTaskQueue::QueueType::kTest:
    case MainThreadTaskQueue::QueueType::kV8:
    case MainThreadTaskQueue::QueueType::kV8UserVisible:
    case MainThreadTaskQueue::QueueType::kV8BestEffort:
    case MainThreadTaskQueue::QueueType::kInput:
    case MainThreadTaskQueue::QueueType::kDetached:
    case MainThreadTaskQueue::QueueType::kNonWaking:
    case MainThreadTaskQueue::QueueType::kOther:
    case MainThreadTaskQueue::QueueType::kIPCTrackingForCachedPages:
      return false;
    case MainThreadTaskQueue::QueueType::kCount:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

MainThreadTaskQueue::MainThreadTaskQueue(
    base::sequence_manager::SequenceManager& sequence_manager,
    const TaskQueue::Spec& spec,
    const QueueCreationParams& params,
    MainThreadSchedulerImpl* main_thread_scheduler)
    : task_queue_(sequence_manager.CreateTaskQueue(spec)),
      queue_type_(params.queue_type),
      queue_traits_(params.queue_traits),
      web_scheduling_queue_type_(params.web_scheduling_queue_type),
      web_scheduling_priority_(params.web_scheduling_priority),
      main_thread_scheduler_(main_thread_scheduler),
      agent_group_scheduler_(params.agent_group_scheduler),
      frame_scheduler_(params.frame_scheduler) {
  task_runner_with_default_task_type_ =
      WrapTaskRunner(task_queue_->task_runner());
  // Throttling needs |should_notify_observers| to get task timing.
  DCHECK(!params.queue_traits.can_be_throttled || spec.should_notify_observers)
      << "Throttled queue is not supported with |!should_notify_observers|";
  DCHECK_EQ(web_scheduling_priority_.has_value(),
            web_scheduling_queue_type_.has_value());
  DCHECK_EQ(web_scheduling_priority_.has_value(),
            queue_type_ == QueueType::kWebScheduling);
  if (spec.should_notify_observers) {
    if (params.queue_traits.can_be_throttled) {
      throttler_.emplace(task_queue_.get(),
                         main_thread_scheduler_->GetTickClock());
    }
    // TODO(scheduler-dev): Consider mapping directly to
    // MainThreadSchedulerImpl::OnTaskStarted/Completed. At the moment this
    // is not possible due to task queue being created inside
    // MainThreadScheduler's constructor.
    task_queue_->SetOnTaskStartedHandler(base::BindRepeating(
        &MainThreadTaskQueue::OnTaskStarted, base::Unretained(this)));
    task_queue_->SetOnTaskCompletedHandler(base::BindRepeating(
        &MainThreadTaskQueue::OnTaskCompleted, base::Unretained(this)));
    task_queue_->SetTaskExecutionTraceLogger(base::BindRepeating(
        &MainThreadTaskQueue::LogTaskExecution, base::Unretained(this)));
  }
}

MainThreadTaskQueue::~MainThreadTaskQueue() {
  DCHECK(!wake_up_budget_pool_);
}

void MainThreadTaskQueue::OnTaskStarted(
    const base::sequence_manager::Task& task,
    const base::sequence_manager::TaskQueue::TaskTiming& task_timing) {
  if (main_thread_scheduler_) {
    main_thread_scheduler_->OnTaskStarted(this, task, task_timing);
  }
}

void MainThreadTaskQueue::OnTaskCompleted(
    const base::sequence_manager::Task& task,
    TaskQueue::TaskTiming* task_timing,
    base::LazyNow* lazy_now) {
  if (main_thread_scheduler_) {
    main_thread_scheduler_->OnTaskCompleted(weak_ptr_factory_.GetWeakPtr(),
                                            task, task_timing, lazy_now);
  }
}

void MainThreadTaskQueue::LogTaskExecution(
    perfetto::EventContext& ctx,
    const base::sequence_manager::Task& task) {
  static const uint8_t* enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED("scheduler");
  if (!*enabled)
    return;
  RendererMainThreadTaskExecution* execution =
      ctx.event<ChromeTrackEvent>()->set_renderer_main_thread_task_execution();
  execution->set_task_type(
      TaskTypeToProto(static_cast<blink::TaskType>(task.task_type)));
  if (frame_scheduler_) {
    frame_scheduler_->WriteIntoTrace(ctx.Wrap(execution));
  }
}

void MainThreadTaskQueue::OnTaskRunTimeReported(
    TaskQueue::TaskTiming* task_timing) {
  if (throttler_) {
    throttler_->OnTaskRunTimeReported(task_timing->start_time(),
                                      task_timing->end_time());
  }
}

void MainThreadTaskQueue::DetachTaskQueue() {
  // The task queue was already shut down, which happens in tests if the
  // `agent_group_scheduler_` is GCed after the task queue impl is unregistered.
  //
  // TODO(crbug.com/1143007): AgentGroupSchedulerImpl should probably not be
  // detaching shut down task queues.
  if (!task_queue_) {
    return;
  }
  // `main_thread_scheduler_` can be null in tests.
  if (!main_thread_scheduler_) {
    return;
  }

  task_queue_->ResetThrottler();
  throttler_.reset();

  // Detach from the underlying scheduler and transfer control to the main
  // thread scheduler.
  agent_group_scheduler_ = nullptr;
  frame_scheduler_ = nullptr;
  on_ipc_task_posted_callback_handle_.reset();
  main_thread_scheduler_->OnDetachTaskQueue(*this);
}

void MainThreadTaskQueue::SetOnIPCTaskPosted(
    base::RepeatingCallback<void(const base::sequence_manager::Task&)>
        on_ipc_task_posted_callback) {
  // We use the frame_scheduler_ to track metrics so as to ensure that metrics
  // are not tied to individual task queues.
  on_ipc_task_posted_callback_handle_ = task_queue_->AddOnTaskPostedHandler(
      std::move(on_ipc_task_posted_callback));
}

void MainThreadTaskQueue::DetachOnIPCTaskPostedWhileInBackForwardCache() {
  on_ipc_task_posted_callback_handle_.reset();
}

void MainThreadTaskQueue::ShutdownTaskQueue() {
  if (main_thread_scheduler_) {
    main_thread_scheduler_->OnShutdownTaskQueue(this);
  }
  main_thread_scheduler_ = nullptr;
  agent_group_scheduler_ = nullptr;
  frame_scheduler_ = nullptr;
  throttler_.reset();
  task_queue_.reset();
}

AgentGroupScheduler* MainThreadTaskQueue::GetAgentGroupScheduler() {
  DCHECK(task_queue_->task_runner()->BelongsToCurrentThread());

  if (agent_group_scheduler_) {
    DCHECK(!frame_scheduler_);
    return agent_group_scheduler_;
  }
  if (frame_scheduler_) {
    return frame_scheduler_->GetAgentGroupScheduler();
  }
  // If this MainThreadTaskQueue was created for MainThreadSchedulerImpl, this
  // queue will not be associated with AgentGroupScheduler or FrameScheduler.
  return nullptr;
}

FrameSchedulerImpl* MainThreadTaskQueue::GetFrameScheduler() const {
  if (!task_queue_) {
    return frame_scheduler_;
  }
  DCHECK(task_queue_->task_runner()->BelongsToCurrentThread());
  return frame_scheduler_;
}

void MainThreadTaskQueue::SetFrameSchedulerForTest(
    FrameSchedulerImpl* frame_scheduler) {
  frame_scheduler_ = frame_scheduler;
}

void MainThreadTaskQueue::SetWebSchedulingPriority(
    WebSchedulingPriority priority) {
  if (web_scheduling_priority_ == priority) {
    return;
  }
  web_scheduling_priority_ = priority;
  if (frame_scheduler_) {
    frame_scheduler_->OnWebSchedulingTaskQueuePriorityChanged(this);
  }
}

void MainThreadTaskQueue::OnWebSchedulingTaskQueueDestroyed() {
  if (frame_scheduler_) {
    frame_scheduler_->OnWebSchedulingTaskQueueDestroyed(this);
  }
}

bool MainThreadTaskQueue::IsThrottled() const {
  if (main_thread_scheduler_) {
    return throttler_.has_value() && throttler_->IsThrottled();
  } else {
    // When the frame detaches the task queue is removed from the throttler.
    return false;
  }
}

MainThreadTaskQueue::ThrottleHandle MainThreadTaskQueue::Throttle() {
  DCHECK(CanBeThrottled());
  return ThrottleHandle(*this);
}

void MainThreadTaskQueue::AddToBudgetPool(base::TimeTicks now,
                                          BudgetPool* pool) {
  pool->AddThrottler(now, &throttler_.value());
}

void MainThreadTaskQueue::RemoveFromBudgetPool(base::TimeTicks now,
                                               BudgetPool* pool) {
  pool->RemoveThrottler(now, &throttler_.value());
}

void MainThreadTaskQueue::SetWakeUpBudgetPool(
    WakeUpBudgetPool* wake_up_budget_pool) {
  wake_up_budget_pool_ = wake_up_budget_pool;
}

void MainThreadTaskQueue::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("type", queue_type_);
  dict.Add("traits", queue_traits_);
  dict.Add("throttler", throttler_);
}

void MainThreadTaskQueue::QueueTraits::WriteIntoTrace(
    perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("can_be_deferred", can_be_deferred);
  dict.Add("can_be_throttled", can_be_throttled);
  dict.Add("can_be_intensively_throttled", can_be_intensively_throttled);
  dict.Add("can_be_paused", can_be_paused);
  dict.Add("can_be_frozen", can_be_frozen);
  dict.Add("can_run_in_background", can_run_in_background);
  dict.Add("can_run_when_virtual_time_paused",
           can_run_when_virtual_time_paused);
  dict.Add("can_be_paused_for_android_webview",
           can_be_paused_for_android_webview);
  dict.Add("prioritisation_type", prioritisation_type);
}

scoped_refptr<base::SingleThreadTaskRunner>
MainThreadTaskQueue::CreateTaskRunner(TaskType task_type) {
  CHECK(task_queue_);
  return WrapTaskRunner(
      task_queue_->CreateTaskRunner(static_cast<int>(task_type)));
}

scoped_refptr<BlinkSchedulerSingleThreadTaskRunner>
MainThreadTaskQueue::WrapTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  // We need to pass the cleanup task runner to task task queues that may stop
  // running tasks before the main thread shuts down as a backup for object
  // deleter tasks.
  scoped_refptr<base::SingleThreadTaskRunner> cleanup_runner =
      main_thread_scheduler_ && (frame_scheduler_ || agent_group_scheduler_)
          ? main_thread_scheduler_->CleanupTaskRunner()
          : nullptr;
  return base::MakeRefCounted<BlinkSchedulerSingleThreadTaskRunner>(
      std::move(task_runner), std::move(cleanup_runner));
}

}  // namespace scheduler
}  // namespace blink
