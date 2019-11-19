// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"

#include <memory>

#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/common/process_state.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/task_queue_throttler.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_scheduler_helper.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_scheduler_proxy.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"

namespace blink {
namespace scheduler {

using base::sequence_manager::TaskQueue;

namespace {

// Worker throttling trial
const char kWorkerThrottlingTrial[] = "BlinkSchedulerDedicatedWorkerThrottling";
const char kWorkerThrottlingMaxBudgetParam[] = "max_budget_ms";
const char kWorkerThrottlingRecoveryRateParam[] = "recovery_rate";
const char kWorkerThrottlingMaxDelayParam[] = "max_delay_ms";

constexpr base::TimeDelta kDefaultMaxBudget = base::TimeDelta::FromSeconds(1);
constexpr double kDefaultRecoveryRate = 0.01;
constexpr base::TimeDelta kDefaultMaxThrottlingDelay =
    base::TimeDelta::FromSeconds(60);

base::Optional<base::TimeDelta> GetMaxBudgetLevel() {
  int max_budget_level_ms;
  if (!base::StringToInt(
          base::GetFieldTrialParamValue(kWorkerThrottlingTrial,
                                        kWorkerThrottlingMaxBudgetParam),
          &max_budget_level_ms)) {
    return kDefaultMaxBudget;
  }
  if (max_budget_level_ms < 0)
    return base::nullopt;
  return base::TimeDelta::FromMilliseconds(max_budget_level_ms);
}

double GetBudgetRecoveryRate() {
  double recovery_rate;
  if (!base::StringToDouble(
          base::GetFieldTrialParamValue(kWorkerThrottlingTrial,
                                        kWorkerThrottlingRecoveryRateParam),
          &recovery_rate)) {
    return kDefaultRecoveryRate;
  }
  return recovery_rate;
}

base::Optional<base::TimeDelta> GetMaxThrottlingDelay() {
  int max_throttling_delay_ms;
  if (!base::StringToInt(
          base::GetFieldTrialParamValue(kWorkerThrottlingTrial,
                                        kWorkerThrottlingMaxDelayParam),
          &max_throttling_delay_ms)) {
    return kDefaultMaxThrottlingDelay;
  }
  if (max_throttling_delay_ms < 0)
    return base::nullopt;
  return base::TimeDelta::FromMilliseconds(max_throttling_delay_ms);
}

}  // namespace

WorkerThreadScheduler::WorkerThreadScheduler(
    ThreadType thread_type,
    base::sequence_manager::SequenceManager* sequence_manager,
    WorkerSchedulerProxy* proxy)
    : NonMainThreadSchedulerImpl(sequence_manager,
                                 TaskType::kWorkerThreadTaskQueueDefault),
      thread_type_(thread_type),
      idle_helper_(helper(),
                   this,
                   "WorkerSchedulerIdlePeriod",
                   base::TimeDelta::FromMilliseconds(300),
                   helper()->NewTaskQueue(TaskQueue::Spec("worker_idle_tq"))),
      lifecycle_state_(proxy ? proxy->lifecycle_state()
                             : SchedulingLifecycleState::kNotThrottled),
      worker_metrics_helper_(thread_type, helper()->HasCPUTimingForEachTask()),
      initial_frame_status_(proxy ? proxy->initial_frame_status()
                                  : FrameStatus::kNone),
      ukm_source_id_(proxy ? proxy->ukm_source_id() : ukm::kInvalidSourceId) {
  if (base::SequencedTaskRunnerHandle::IsSet()) {
    mojo::PendingRemote<ukm::mojom::UkmRecorderInterface> recorder;
    Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        recorder.InitWithNewPipeAndPassReceiver());
    ukm_recorder_ = std::make_unique<ukm::MojoUkmRecorder>(std::move(recorder));
  }

  if (proxy && proxy->parent_frame_type())
    worker_metrics_helper_.SetParentFrameType(*proxy->parent_frame_type());

  if (thread_type == ThreadType::kDedicatedWorkerThread &&
      base::FeatureList::IsEnabled(kDedicatedWorkerThrottling)) {
    CreateTaskQueueThrottler();
  }

  TRACE_EVENT_OBJECT_CREATED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("worker.scheduler"), "WorkerScheduler", this);
}

WorkerThreadScheduler::~WorkerThreadScheduler() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("worker.scheduler"), "WorkerScheduler", this);

  DCHECK(worker_schedulers_.IsEmpty());
}

scoped_refptr<SingleThreadIdleTaskRunner>
WorkerThreadScheduler::IdleTaskRunner() {
  DCHECK(initialized_);
  return idle_helper_.IdleTaskRunner();
}

scoped_refptr<base::SingleThreadTaskRunner>
WorkerThreadScheduler::V8TaskRunner() {
  DCHECK(initialized_);
  return v8_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
WorkerThreadScheduler::CompositorTaskRunner() {
  DCHECK(initialized_);
  return compositor_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
WorkerThreadScheduler::IPCTaskRunner() {
  NOTREACHED() << "Not implemented";
  return nullptr;
}

bool WorkerThreadScheduler::CanExceedIdleDeadlineIfRequired() const {
  DCHECK(initialized_);
  return idle_helper_.CanExceedIdleDeadlineIfRequired();
}

bool WorkerThreadScheduler::ShouldYieldForHighPriorityWork() {
  // We don't consider any work as being high priority on workers.
  return false;
}

void WorkerThreadScheduler::AddTaskObserver(base::TaskObserver* task_observer) {
  DCHECK(initialized_);
  helper()->AddTaskObserver(task_observer);
}

void WorkerThreadScheduler::RemoveTaskObserver(
    base::TaskObserver* task_observer) {
  DCHECK(initialized_);
  helper()->RemoveTaskObserver(task_observer);
}

void WorkerThreadScheduler::Shutdown() {
  DCHECK(initialized_);
  task_queue_throttler_.reset();
  idle_helper_.Shutdown();
  helper()->Shutdown();
}

scoped_refptr<NonMainThreadTaskQueue>
WorkerThreadScheduler::DefaultTaskQueue() {
  DCHECK(initialized_);
  return helper()->DefaultNonMainThreadTaskQueue();
}

void WorkerThreadScheduler::InitImpl() {
  initialized_ = true;
  idle_helper_.EnableLongIdlePeriod();

  v8_task_runner_ =
      DefaultTaskQueue()->CreateTaskRunner(TaskType::kWorkerThreadTaskQueueV8);
  compositor_task_runner_ = DefaultTaskQueue()->CreateTaskRunner(
      TaskType::kWorkerThreadTaskQueueCompositor);
}

void WorkerThreadScheduler::OnTaskCompleted(
    NonMainThreadTaskQueue* task_queue,
    const base::sequence_manager::Task& task,
    TaskQueue::TaskTiming* task_timing,
    base::sequence_manager::LazyNow* lazy_now) {
  PerformMicrotaskCheckpoint();

  task_timing->RecordTaskEnd(lazy_now);
  worker_metrics_helper_.RecordTaskMetrics(task_queue, task, *task_timing);

  if (task_queue_throttler_) {
    task_queue_throttler_->OnTaskRunTimeReported(
        task_queue, task_timing->start_time(), task_timing->end_time());
  }

  RecordTaskUkm(task_queue, task, *task_timing);
}

SchedulerHelper* WorkerThreadScheduler::GetSchedulerHelperForTesting() {
  return helper();
}

bool WorkerThreadScheduler::CanEnterLongIdlePeriod(base::TimeTicks,
                                                   base::TimeDelta*) {
  return true;
}

base::TimeTicks WorkerThreadScheduler::CurrentIdleTaskDeadlineForTesting()
    const {
  return idle_helper_.CurrentIdleTaskDeadline();
}

void WorkerThreadScheduler::OnLifecycleStateChanged(
    SchedulingLifecycleState lifecycle_state) {
  if (lifecycle_state_ == lifecycle_state)
    return;
  lifecycle_state_ = lifecycle_state;

  for (WorkerScheduler* worker_scheduler : worker_schedulers_)
    worker_scheduler->OnLifecycleStateChanged(lifecycle_state);
}

void WorkerThreadScheduler::RegisterWorkerScheduler(
    WorkerScheduler* worker_scheduler) {
  worker_schedulers_.insert(worker_scheduler);
  worker_scheduler->OnLifecycleStateChanged(lifecycle_state_);
}

void WorkerThreadScheduler::UnregisterWorkerScheduler(
    WorkerScheduler* worker_scheduler) {
  DCHECK(worker_schedulers_.find(worker_scheduler) != worker_schedulers_.end());
  worker_schedulers_.erase(worker_scheduler);
}

scoped_refptr<NonMainThreadTaskQueue>
WorkerThreadScheduler::ControlTaskQueue() {
  return helper()->ControlNonMainThreadTaskQueue();
}

void WorkerThreadScheduler::CreateTaskQueueThrottler() {
  if (task_queue_throttler_)
    return;
  task_queue_throttler_ = std::make_unique<TaskQueueThrottler>(
      this, &traceable_variable_controller_);
  wake_up_budget_pool_ =
      task_queue_throttler_->CreateWakeUpBudgetPool("worker_wake_up_pool");
  cpu_time_budget_pool_ =
      task_queue_throttler_->CreateCPUTimeBudgetPool("worker_cpu_time_pool");

  base::TimeTicks now = GetTickClock()->NowTicks();
  cpu_time_budget_pool_->SetMaxBudgetLevel(now, GetMaxBudgetLevel());
  cpu_time_budget_pool_->SetTimeBudgetRecoveryRate(now,
                                                   GetBudgetRecoveryRate());
  cpu_time_budget_pool_->SetMaxThrottlingDelay(now, GetMaxThrottlingDelay());
}

void WorkerThreadScheduler::RecordTaskUkm(
    NonMainThreadTaskQueue* worker_task_queue,
    const base::sequence_manager::Task& task,
    const base::sequence_manager::TaskQueue::TaskTiming& task_timing) {
  if (!helper()->ShouldRecordTaskUkm(task_timing.has_thread_time()))
    return;
  ukm::builders::RendererSchedulerTask builder(ukm_source_id_);

  builder.SetVersion(kUkmMetricVersion);
  builder.SetThreadType(static_cast<int>(thread_type_));

  builder.SetRendererBackgrounded(
      internal::ProcessState::Get()->is_process_backgrounded);
  builder.SetTaskType(task.task_type);
  builder.SetFrameStatus(static_cast<int>(initial_frame_status_));
  builder.SetTaskDuration(task_timing.wall_duration().InMicroseconds());

  if (task_timing.has_thread_time())
    builder.SetTaskCPUDuration(task_timing.thread_duration().InMicroseconds());

  builder.Record(ukm_recorder_.get());
}

void WorkerThreadScheduler::SetUkmRecorderForTest(
    std::unique_ptr<ukm::UkmRecorder> ukm_recorder) {
  ukm_recorder_ = std::move(ukm_recorder);
}

void WorkerThreadScheduler::SetUkmTaskSamplingRateForTest(double rate) {
  helper()->SetUkmTaskSamplingRateForTest(rate);
}

void WorkerThreadScheduler::SetCPUTimeBudgetPoolForTesting(
    CPUTimeBudgetPool* cpu_time_budget_pool) {
  cpu_time_budget_pool_ = cpu_time_budget_pool;
}

HashSet<WorkerScheduler*>&
WorkerThreadScheduler::GetWorkerSchedulersForTesting() {
  return worker_schedulers_;
}

void WorkerThreadScheduler::PerformMicrotaskCheckpoint() {
  if (isolate())
    EventLoop::PerformIsolateGlobalMicrotasksCheckpoint(isolate());
}

}  // namespace scheduler
}  // namespace blink
