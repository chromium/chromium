// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/scheduler/common/auto_advancing_virtual_time_domain.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/common/metrics_helper.h"
#include "third_party/blink/renderer/platform/scheduler/common/process_state.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/cpu_time_budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/task_queue_throttler.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/wake_up_budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_scheduler_helper.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_scheduler_proxy.h"

namespace blink {
namespace scheduler {

using base::sequence_manager::TaskQueue;

namespace {

// Worker throttling trial
const char kWorkerThrottlingTrial[] = "BlinkSchedulerDedicatedWorkerThrottling";
const char kWorkerThrottlingMaxBudgetParam[] = "max_budget_ms";
const char kWorkerThrottlingRecoveryRateParam[] = "recovery_rate";
const char kWorkerThrottlingMaxDelayParam[] = "max_delay_ms";

constexpr base::TimeDelta kDefaultMaxBudget = base::Seconds(1);
constexpr double kDefaultRecoveryRate = 0.01;
constexpr base::TimeDelta kDefaultMaxThrottlingDelay = base::Seconds(60);

std::optional<base::TimeDelta> GetMaxBudgetLevel() {
  int max_budget_level_ms;
  if (!base::StringToInt(
          base::GetFieldTrialParamValue(kWorkerThrottlingTrial,
                                        kWorkerThrottlingMaxBudgetParam),
          &max_budget_level_ms)) {
    return kDefaultMaxBudget;
  }
  if (max_budget_level_ms < 0)
    return std::nullopt;
  return base::Milliseconds(max_budget_level_ms);
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

std::optional<base::TimeDelta> GetMaxThrottlingDelay() {
  int max_throttling_delay_ms;
  if (!base::StringToInt(
          base::GetFieldTrialParamValue(kWorkerThrottlingTrial,
                                        kWorkerThrottlingMaxDelayParam),
          &max_throttling_delay_ms)) {
    return kDefaultMaxThrottlingDelay;
  }
  if (max_throttling_delay_ms < 0)
    return std::nullopt;
  return base::Milliseconds(max_throttling_delay_ms);
}

std::unique_ptr<ukm::MojoUkmRecorder> CreateMojoUkmRecorder() {
  mojo::Remote<ukm::mojom::UkmRecorderFactory> factory;
  Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      factory.BindNewPipeAndPassReceiver());
  return ukm::MojoUkmRecorder::Create(*factory);
}

}  // namespace

WorkerThreadScheduler::WorkerThreadScheduler(
    ThreadType thread_type,
    base::sequence_manager::SequenceManager* sequence_manager,
    WorkerSchedulerProxy* proxy)
    : NonMainThreadSchedulerBase(sequence_manager,
                                 TaskType::kWorkerThreadTaskQueueDefault),
      thread_type_(thread_type),
      idle_helper_queue_(GetHelper().NewTaskQueue(
          TaskQueue::Spec(base::sequence_manager::QueueName::WORKER_IDLE_TQ))),
      idle_helper_(&GetHelper(),
                   this,
                   "WorkerSchedulerIdlePeriod",
                   base::Milliseconds(300),
                   idle_helper_queue_->GetTaskQueue()),
      lifecycle_state_(proxy ? proxy->lifecycle_state()
                             : SchedulingLifecycleState::kNotThrottled),
      initial_frame_status_(proxy ? proxy->initial_frame_status()
                                  : FrameStatus::kNone),
      ukm_source_id_(proxy ? proxy->ukm_source_id() : ukm::kInvalidSourceId) {
  if (thread_type == ThreadType::kDedicatedWorkerThread &&
      base::FeatureList::IsEnabled(kDedicatedWorkerThrottling)) {
    CreateBudgetPools();
  }

  GetHelper().SetObserver(this);

  TRACE_EVENT_OBJECT_CREATED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("worker.scheduler"), "WorkerScheduler", this);
}

WorkerThreadScheduler::~WorkerThreadScheduler() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("worker.scheduler"), "WorkerScheduler", this);

  DCHECK(worker_schedulers_.empty());
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
WorkerThreadScheduler::CleanupTaskRunner() {
  return DefaultTaskQueue()->GetTaskRunnerWithDefaultTaskType();
}

scoped_refptr<base::SingleThreadTaskRunner>
WorkerThreadScheduler::CompositorTaskRunner() {
  DCHECK(initialized_);
  return compositor_task_runner_;
}

bool WorkerThreadScheduler::ShouldYieldForHighPriorityWork() {
  // We don't consider any work as being high priority on workers.
  return false;
}

void WorkerThreadScheduler::AddTaskObserver(base::TaskObserver* task_observer) {
  DCHECK(initialized_);
  GetHelper().AddTaskObserver(task_observer);
}

void WorkerThreadScheduler::RemoveTaskObserver(
    base::TaskObserver* task_observer) {
  DCHECK(initialized_);
  GetHelper().RemoveTaskObserver(task_observer);
}

void WorkerThreadScheduler::Shutdown() {
  DCHECK(initialized_);
  ThreadSchedulerBase::Shutdown();
  idle_helper_.Shutdown();
  idle_helper_queue_->ShutdownTaskQueue();
  GetHelper().Shutdown();
}

scoped_refptr<NonMainThreadTaskQueue>
WorkerThreadScheduler::DefaultTaskQueue() {
  DCHECK(initialized_);
  return GetHelper().DefaultNonMainThreadTaskQueue();
}

void WorkerThreadScheduler::Init() {
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
    base::LazyNow* lazy_now) {
  PerformMicrotaskCheckpoint();

  task_timing->RecordTaskEnd(lazy_now);
  DispatchOnTaskCompletionCallbacks();

  if (task_queue != nullptr)
    task_queue->OnTaskRunTimeReported(task_timing);

  RecordTaskUkm(task_queue, task, *task_timing);
}

SchedulerHelper* WorkerThreadScheduler::GetSchedulerHelperForTesting() {
  return &GetHelper();
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
    WorkerSchedulerImpl* worker_scheduler) {
  worker_schedulers_.insert(worker_scheduler);
  worker_scheduler->OnLifecycleStateChanged(lifecycle_state_);
}

void WorkerThreadScheduler::UnregisterWorkerScheduler(
    WorkerSchedulerImpl* worker_scheduler) {
  DCHECK(base::Contains(worker_schedulers_, worker_scheduler));
  worker_schedulers_.erase(worker_scheduler);
}

scoped_refptr<NonMainThreadTaskQueue>
WorkerThreadScheduler::ControlTaskQueue() {
  return GetHelper().ControlNonMainThreadTaskQueue();
}

void WorkerThreadScheduler::CreateBudgetPools() {
  if (wake_up_budget_pool_ && cpu_time_budget_pool_)
    return;
  base::TimeTicks now = GetTickClock()->NowTicks();
  wake_up_budget_pool_ =
      std::make_unique<WakeUpBudgetPool>("worker_wake_up_pool");
  cpu_time_budget_pool_ = std::make_unique<CPUTimeBudgetPool>(
      "worker_cpu_time_pool", &traceable_variable_controller_, now);

  cpu_time_budget_pool_->SetMaxBudgetLevel(now, GetMaxBudgetLevel());
  cpu_time_budget_pool_->SetTimeBudgetRecoveryRate(now,
                                                   GetBudgetRecoveryRate());
  cpu_time_budget_pool_->SetMaxThrottlingDelay(now, GetMaxThrottlingDelay());
}

void WorkerThreadScheduler::RecordTaskUkm(
    NonMainThreadTaskQueue* worker_task_queue,
    const base::sequence_manager::Task& task,
    const base::sequence_manager::TaskQueue::TaskTiming& task_timing) {
  if (!GetHelper().ShouldRecordTaskUkm(task_timing.has_thread_time()))
    return;

  if (!ukm_recorder_)
    ukm_recorder_ = CreateMojoUkmRecorder();

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
  GetHelper().SetUkmTaskSamplingRateForTest(rate);
}

void WorkerThreadScheduler::SetCPUTimeBudgetPoolForTesting(
    std::unique_ptr<CPUTimeBudgetPool> cpu_time_budget_pool) {
  cpu_time_budget_pool_ = std::move(cpu_time_budget_pool);
}

HashSet<WorkerSchedulerImpl*>&
WorkerThreadScheduler::GetWorkerSchedulersForTesting() {
  return worker_schedulers_;
}

void WorkerThreadScheduler::PerformMicrotaskCheckpoint() {
  if (isolate())
    EventLoop::PerformIsolateGlobalMicrotasksCheckpoint(isolate());
}

base::SequencedTaskRunner* WorkerThreadScheduler::GetVirtualTimeTaskRunner() {
  // Note this is not Control task runner because it has task notifications
  // disabled.
  return DefaultTaskQueue()->GetTaskRunnerWithDefaultTaskType().get();
}

void WorkerThreadScheduler::OnVirtualTimeDisabled() {}

void WorkerThreadScheduler::OnVirtualTimePaused() {
  for (auto* worker_scheduler : worker_schedulers_) {
    worker_scheduler->PauseVirtualTime();
  }
}

void WorkerThreadScheduler::OnVirtualTimeResumed() {
  for (WorkerScheduler* worker_scheduler : worker_schedulers_) {
    auto* scheduler = static_cast<WorkerSchedulerImpl*>(worker_scheduler);
    scheduler->UnpauseVirtualTime();
  }
}

void WorkerThreadScheduler::PostIdleTask(const base::Location& location,
                                         Thread::IdleTask task) {
  IdleTaskRunner()->PostIdleTask(location, std::move(task));
}

void WorkerThreadScheduler::PostNonNestableIdleTask(
    const base::Location& location,
    Thread::IdleTask task) {
  IdleTaskRunner()->PostNonNestableIdleTask(location, std::move(task));
}

void WorkerThreadScheduler::PostDelayedIdleTask(const base::Location& location,
                                                base::TimeDelta delay,
                                                Thread::IdleTask task) {
  IdleTaskRunner()->PostDelayedIdleTask(location, delay, std::move(task));
}

base::TimeTicks WorkerThreadScheduler::MonotonicallyIncreasingVirtualTime() {
  return base::TimeTicks::Now();
}

void WorkerThreadScheduler::SetV8Isolate(v8::Isolate* isolate) {
  NonMainThreadSchedulerBase::SetV8Isolate(isolate);
}

}  // namespace scheduler
}  // namespace blink
