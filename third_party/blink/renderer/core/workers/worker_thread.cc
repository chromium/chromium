/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/core/workers/worker_thread.h"

#include <limits>
#include <memory>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "third_party/blink/public/common/loader/worker_main_script_load_parameters.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/policy_container.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue_storage.h"
#include "third_party/blink/renderer/core/inspector/inspector_task_runner.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"
#include "third_party/blink/renderer/core/inspector/worker_inspector_controller.h"
#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"
#include "third_party/blink/renderer/core/loader/worker_resource_timing_notifier_impl.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/cross_thread_global_scope_creation_params_copier.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/worker_resource_timing_notifier.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_impl.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

using ExitCode = WorkerThread::ExitCode;

namespace {

constexpr base::TimeDelta kForcibleTerminationDelay = base::Seconds(2);

}  // namespace

base::Lock& WorkerThread::ThreadSetLock() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(base::Lock, lock, ());
  return lock;
}

static std::atomic_int g_unique_worker_thread_id(1);

static int GetNextWorkerThreadId() {
  int next_worker_thread_id =
      g_unique_worker_thread_id.fetch_add(1, std::memory_order_relaxed);
  CHECK_LT(next_worker_thread_id, std::numeric_limits<int>::max());
  return next_worker_thread_id;
}

// RefCountedWaitableEvent makes WaitableEvent thread-safe refcounted.
// WorkerThread retains references to the event from both the parent context
// thread and the worker thread with this wrapper. See
// WorkerThread::PerformShutdownOnWorkerThread() for details.
class WorkerThread::RefCountedWaitableEvent
    : public WTF::ThreadSafeRefCounted<RefCountedWaitableEvent> {
 public:
  static scoped_refptr<RefCountedWaitableEvent> Create() {
    return base::AdoptRef<RefCountedWaitableEvent>(new RefCountedWaitableEvent);
  }

  RefCountedWaitableEvent(const RefCountedWaitableEvent&) = delete;
  RefCountedWaitableEvent& operator=(const RefCountedWaitableEvent&) = delete;

  void Wait() { event_.Wait(); }
  void Signal() { event_.Signal(); }

 private:
  RefCountedWaitableEvent() = default;

  base::WaitableEvent event_;
};

// A class that is passed into V8 Interrupt and via a PostTask. Once both have
// run this object will be destroyed in
// PauseOrFreezeWithInterruptDataOnWorkerThread. The V8 API only takes a raw ptr
// otherwise this could have been done with WTF::Bind and ref counted objects.
class WorkerThread::InterruptData {
 public:
  InterruptData(WorkerThread* worker_thread,
                mojom::blink::FrameLifecycleState state,
                bool is_in_back_forward_cache)
      : worker_thread_(worker_thread),
        state_(state),
        is_in_back_forward_cache_(is_in_back_forward_cache) {
    DCHECK(!is_in_back_forward_cache ||
           state == mojom::blink::FrameLifecycleState::kFrozen);
  }

  InterruptData(const InterruptData&) = delete;
  InterruptData& operator=(const InterruptData&) = delete;

  bool ShouldRemoveFromList() { return seen_interrupt_ && seen_post_task_; }
  void MarkPostTaskCalled() { seen_post_task_ = true; }
  void MarkInterruptCalled() { seen_interrupt_ = true; }

  mojom::blink::FrameLifecycleState state() { return state_; }
  WorkerThread* worker_thread() { return worker_thread_; }
  bool is_in_back_forward_cache() const { return is_in_back_forward_cache_; }

 private:
  WorkerThread* worker_thread_;
  mojom::blink::FrameLifecycleState state_;
  bool is_in_back_forward_cache_;
  bool seen_interrupt_ = false;
  bool seen_post_task_ = false;
};

WorkerThread::~WorkerThread() {
  DCHECK_CALLED_ON_VALID_THREAD(parent_thread_checker_);
  base::AutoLock locker(ThreadSetLock());
  DCHECK(InitializingWorkerThreads().Contains(this) ||
         WorkerThreads().Contains(this));
  InitializingWorkerThreads().erase(this);
  WorkerThreads().erase(this);

  DCHECK(child_threads_.empty());
  DCHECK_NE(ExitCode::kNotTerminated, exit_code_);
}

void WorkerThread::Start(
    std::unique_ptr<GlobalScopeCreationParams> global_scope_creation_params,
    const std::optional<WorkerBackingThreadStartupData>& thread_startup_data,
    std::unique_ptr<WorkerDevToolsParams> devtools_params) {
  DCHECK_CALLED_ON_VALID_THREAD(parent_thread_checker_);
  devtools_worker_token_ = devtools_params->devtools_worker_token;

  // Synchronously initialize the per-global-scope scheduler to prevent someone
  // from posting a task to the thread before the scheduler is ready.
  base::WaitableEvent waitable_event;
  PostCrossThreadTask(
      *GetWorkerBackingThread().BackingThread().GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&WorkerThread::InitializeSchedulerOnWorkerThread,
                          CrossThreadUnretained(this),
                          CrossThreadUnretained(&waitable_event)));
  {
    base::ScopedAllowBaseSyncPrimitives allow_wait;
    waitable_event.Wait();
  }

  inspector_task_runner_ =
      InspectorTaskRunner::Create(GetTaskRunner(TaskType::kInternalInspector));

  PostCrossThreadTask(
      *GetWorkerBackingThread().BackingThread().GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&WorkerThread::InitializeOnWorkerThread,
                          CrossThreadUnretained(this),
                          std::move(global_scope_creation_params),
                          IsOwningBackingThread() ?
                              thread_startup_data : std::nullopt,
                          std::move(devtools_params)));
}

void WorkerThread::EvaluateClassicScript(
    const KURL& script_url,
    const String& source_code,
    std::unique_ptr<Vector<uint8_t>> cached_meta_data,
    const v8_inspector::V8StackTraceId& stack_id) {
  DCHECK_CALLED_ON_VALID_THREAD(parent_thread_checker_);
  PostCrossThreadTask(
      *GetTaskRunner(TaskType::kDOMManipulation), FROM_HERE,
      CrossThreadBindOnce(&WorkerThread::EvaluateClassicScriptOnWorkerThread,
                          CrossThreadUnretained(this), script_url, source_code,
                          std::move(cached_meta_data), stack_id));
}

void WorkerThread::FetchAndRunClassicScript(
    const KURL& script_url,
    std::unique_ptr<WorkerMainScriptLoadParameters>
        worker_main_script_load_params,
    std::unique_ptr<WebPolicyContainer> policy_container,
    std::unique_ptr<CrossThreadFetchClientSettingsObjectData>
        outside_settings_object_data,
    WorkerResourceTimingNotifier* outside_resource_timing_notifier,
    const v8_inspector::V8StackTraceId& stack_id) {
  DCHECK_CALLED_ON_VALID_THREAD(parent_thread_checker_);
  PostCrossThreadTask(
      *GetTaskRunner(TaskType::kDOMManipulation), FROM_HERE,
      CrossThreadBindOnce(
          &WorkerThread::FetchAndRunClassicScriptOnWorkerThread,
          CrossThreadUnretained(this), script_url,
          std::move(worker_main_script_load_params),
          std::move(policy_container), std::move(outside_settings_object_data),
          WrapCrossThreadPersistent(outside_resource_timing_notifier),
          stack_id));
}

void WorkerThread::FetchAndRunModuleScript(
    const KURL& script_url,
    std::unique_ptr<WorkerMainScriptLoadParameters>
        worker_main_script_load_params,
    std::unique_ptr<WebPolicyContainer> policy_container,
    std::unique_ptr<CrossThreadFetchClientSettingsObjectData>
        outside_settings_object_data,
    WorkerResourceTimingNotifier* outside_resource_timing_notifier,
    network::mojom::CredentialsMode credentials_mode,
    RejectCoepUnsafeNone reject_coep_unsafe_none) {
  DCHECK_CALLED_ON_VALID_THREAD(parent_thread_checker_);
  PostCrossThreadTask(
      *GetTaskRunner(TaskType::kDOMManipulation), FROM_HERE,
      CrossThreadBindOnce(
          &WorkerThread::FetchAndRunModuleScriptOnWorkerThread,
          CrossThreadUnretained(this), script_url,
          std::move(worker_main_script_load_params),
          std::move(policy_container), std::move(outside_settings_object_data),
          WrapCrossThreadPersistent(outside_resource_timing_notifier),
          credentials_mode, reject_coep_unsafe_none.value()));
}

void WorkerThread::Pause() {
  PauseOrFreeze(mojom::blink::FrameLifecycleState::kPaused, false);
}

void WorkerThread::Freeze(bool is_in_back_forward_cache) {
  PauseOrFreeze(mojom::blink::FrameLifecycleState::kFrozen,
                is_in_back_forward_cache);
}

void WorkerThread::Resume() {
  // Might be called from any thread.
  if (IsCurrentThread()) {
    ResumeOnWorkerThread();
  } else {
    PostCrossThreadTask(
        *GetWorkerBackingThread().BackingThread().GetTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(&WorkerThread::ResumeOnWorkerThread,
                            CrossThreadUnretained(this)));
  }
}

void WorkerThread::Terminate() {
  DCHECK_CALLED_ON_VALID_THREAD(parent_thread_checker_);
  {
    base::AutoLock locker(lock_);
    if (requested_to_terminate_)
      return;
    requested_to_terminate_ = true;
  }

  // Schedule a task to forcibly terminate the script execution in case that the
  // shutdown sequence does not start on the worker thread in a certain time
  // period.
  ScheduleToTerminateScriptExecution();

  inspector_task_runner_->Dispose();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetWorkerBackingThread().BackingThread().GetTaskRunner();
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(&WorkerThread::PrepareForShutdownOnWorkerThread,
                          CrossThreadUnretained(this)));
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(&WorkerThread::PerformShutdownOnWorkerThread,
                          CrossThreadUnretained(this)));
}

void WorkerThread::TerminateForTesting() {
  // Schedule a regular async worker thread termination task, and forcibly
  // terminate the V8 script execution to ensure the task runs.
  Terminate();
  EnsureScriptExecutionTerminates(ExitCode::kSyncForciblyTerminated);
}

void WorkerThread::WillProcessTask(const base::PendingTask& pending_task,
                                   bool was_blocked_or_low_priority) {
  DCHECK(IsCurrentThread());

  // No tasks should get executed after we have closed.
  DCHECK(!GlobalScope()->IsClosing());
}

void WorkerThread::DidProcessTask(const base::PendingTask& pending_task) {
  DCHECK(IsCurrentThread());

  // TODO(tzik): Move this to WorkerThreadScheduler::OnTaskCompleted(), so that
  // metrics for microtasks are counted as a part of the preceding task.
  GlobalScope()->GetAgent()->event_loop()->PerformMicrotaskCheckpoint();

  // EventLoop::PerformIsolateGlobalMicrotasksCheckpoint() runs microtasks and
  // its completion hooks for the default microtask queue. The default queue may
  // contain the microtasks queued by V8 itself, and legacy
  // blink::MicrotaskQueue::EnqueueMicrotask. The completion hook contains
  // IndexedDB clean-up task, as described at
  // https://html.spec.whatwg.org/C#perform-a-microtask-checkpoint
  // TODO(tzik): Move rejected promise handling to EventLoop.

  GlobalScope()->ScriptController()->GetRejectedPromises()->ProcessQueue();
  if (GlobalScope()->IsClosing()) {
    // This WorkerThread will eventually be requested to terminate.
    GetWorkerReportingProxy().DidCloseWorkerGlobalScope();

    // Stop further worker tasks to run after this point based on the spec:
    // https://html.spec.whatwg.org/C/#close-a-worker
    //
    // "To close a worker, given a workerGlobal, run these steps:"
    // Step 1: "Discard any tasks that have been added to workerGlobal's event
    // loop's task queues."
    // Step 2: "Set workerGlobal's closing flag to true. (This prevents any
    // further tasks from being queued.)"
    PrepareForShutdownOnWorkerThread();
  } else if (IsForciblyTerminated()) {
    // The script has been terminated forcibly, which means we need to
    // ask objects in the thread to stop working as soon as possible.
    PrepareForShutdownOnWorkerThread();
  }
}

v8::Isolate* WorkerThread::GetIsolate() {
  return GetWorkerBackingThread().GetIsolate();
}

bool WorkerThread::IsCurrentThread() {
  return GetWorkerBackingThread().BackingThread().IsCurrentThread();
}

void WorkerThread::DebuggerTaskStarted() {
  base::AutoLock locker(lock_);
  DCHECK(IsCurrentThread());
  debugger_task_counter_++;
}

void WorkerThread::DebuggerTaskFinished() {
  base::AutoLock locker(lock_);
  DCHECK(IsCurrentThread());
  debugger_task_counter_--;
}

WorkerOrWorkletGlobalScope* WorkerThread::GlobalScope() {
  DCHECK(IsCurrentThread());
  return global_scope_.Get();
}

WorkerInspectorController* WorkerThread::GetWorkerInspectorController() {
  DCHECK(IsCurrentThread());
  return worker_inspector_controller_.Get();
}

unsigned WorkerThread::WorkerThreadCount() {
  base::AutoLock locker(ThreadSetLock());
  return InitializingWorkerThreads().size() + WorkerThreads().size();
}

HashSet<WorkerThread*>& WorkerThread::InitializingWorkerThreads() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(HashSet<WorkerThread*>, threads, ());
  return threads;
}

HashSet<WorkerThread*>& WorkerThread::WorkerThreads() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(HashSet<WorkerThread*>, threads, ());
  return threads;
}

bool WorkerThread::IsForciblyTerminated() {
  base::AutoLock locker(lock_);
  switch (exit_code_) {
    case ExitCode::kNotTerminated:
    case ExitCode::kGracefullyTerminated:
      return false;
    case ExitCode::kSyncForciblyTerminated:
    case ExitCode::kAsyncForciblyTerminated:
      return true;
  }
  NOTREACHED_IN_MIGRATION() << static_cast<int>(exit_code_);
  return false;
}

void WorkerThread::WaitForShutdownForTesting() {
  DCHECK_CALLED_ON_VALID_THREAD(parent_thread_checker_);
  base::ScopedAllowBaseSyncPrimitives allow_wait;
  shutdown_event_->Wait();
}

ExitCode WorkerThread::GetExitCodeForTesting() {
  base::AutoLock locker(lock_);
  return exit_code_;
}

scheduler::WorkerScheduler* WorkerThread::GetScheduler() {
  DCHECK(IsCurrentThread());
  return worker_scheduler_.get();
}

scoped_refptr<base::SingleThreadTaskRunner> WorkerThread::GetTaskRunner(
    TaskType type) {
  // Task runners must be captured when the worker scheduler is initialized. See
  // comments in InitializeSchedulerOnWorkerThread().
  CHECK(worker_task_runners_.Contains(type)) << static_cast<int>(type);
  return worker_task_runners_.at(type);
}

void WorkerThread::ChildThreadStartedOnWorkerThread(WorkerThread* child) {
  DCHECK(IsCurrentThread());
#if DCHECK_IS_ON()
  {
    base::AutoLock locker(lock_);
    DCHECK_EQ(ThreadState::kRunning, thread_state_);
  }
#endif
  child_threads_.insert(child);
}

void WorkerThread::ChildThreadTerminatedOnWorkerThread(WorkerThread* child) {
  DCHECK(IsCurrentThread());
  child_threads_.erase(child);
  if (child_threads_.empty() && CheckRequestedToTerminate())
    PerformShutdownOnWorkerThread();
}

WorkerThread::WorkerThread(WorkerReportingProxy& worker_reporting_proxy)
    : WorkerThread(worker_reporting_proxy,
                   ThreadScheduler::Current()->CleanupTaskRunner()) {}

WorkerThread::WorkerThread(WorkerReportingProxy& worker_reporting_proxy,
                           scoped_refptr<base::SingleThreadTaskRunner>
                               parent_thread_default_task_runner)
    : time_origin_(base::TimeTicks::Now()),
      worker_thread_id_(GetNextWorkerThreadId()),
      forcible_termination_delay_(kForcibleTerminationDelay),
      worker_reporting_proxy_(worker_reporting_proxy),
      parent_thread_default_task_runner_(
          std::move(parent_thread_default_task_runner)),
      shutdown_event_(RefCountedWaitableEvent::Create()) {
  DCHECK_CALLED_ON_VALID_THREAD(parent_thread_checker_);
  base::AutoLock locker(ThreadSetLock());
  InitializingWorkerThreads().insert(this);
}

void WorkerThread::ScheduleToTerminateScriptExecution() {
  DCHECK_CALLED_ON_VALID_THREAD(parent_thread_checker_);
  DCHECK(!forcible_termination_task_handle_.IsActive());
  // It's safe to post a task bound with |this| to the parent thread default
  // task runner because this task is canceled on the destructor of this
  // class on the parent thread.
  forcible_termination_task_handle_ = PostDelayedCancellableTask(
      *parent_thread_default_task_runner_, FROM_HERE,
      WTF::BindOnce(&WorkerThread::EnsureScriptExecutionTerminates,
                    WTF::Unretained(this), ExitCode::kAsyncForciblyTerminated),
      forcible_termination_delay_);
}

WorkerThread::TerminationState WorkerThread::ShouldTerminateScriptExecution() {
  DCHECK_CALLED_ON_VALID_THREAD(parent_thread_checker_);
  switch (thread_state_) {
    case ThreadState::kNotStarted:
      // Shutdown sequence will surely start during initialization sequence
      // on the worker thread. Don't have to schedule a termination task.
      return TerminationState::kTerminationUnnecessary;
    case ThreadState::kRunning:
      // Terminating during debugger task may lead to crash due to heavy use
      // of v8 api in debugger. Any debugger task is guaranteed to finish, so
      // we can wait for the completion.
      return debugger_task_counter_ > 0 ? TerminationState::kPostponeTerminate
                                        : TerminationState::kTerminate;
    case ThreadState::kReadyToShutdown:
      // Shutdown sequence might have started in a nested event loop but
      // JS might continue running after it exits the nested loop.
      return exit_code_ == ExitCode::kNotTerminated
                 ? TerminationState::kTerminate
                 : TerminationState::kTerminationUnnecessary;
  }
  NOTREACHED_IN_MIGRATION();
  return TerminationState::kTerminationUnnecessary;
}

void WorkerThread::EnsureScriptExecutionTerminates(ExitCode exit_code) {
  DCHECK_CALLED_ON_VALID_THREAD(parent_thread_checker_);
  base::AutoLock locker(lock_);
  switch (ShouldTerminateScriptExecution()) {
    case TerminationState::kTerminationUnnecessary:
      return;
    case TerminationState::kTerminate:
      break;
    case TerminationState::kPostponeTerminate:
      ScheduleToTerminateScriptExecution();
      return;
  }

  DCHECK(exit_code == ExitCode::kSyncForciblyTerminated ||
         exit_code == ExitCode::kAsyncForciblyTerminated);
  SetExitCode(exit_code);

  GetIsolate()->TerminateExecution();
  forcible_termination_task_handle_.Cancel();
}

void WorkerThread::InitializeSchedulerOnWorkerThread(
    base::WaitableEvent* waitable_event) {
  DCHECK(IsCurrentThread());
  DCHECK(!worker_scheduler_);

  // TODO(hajimehoshi, nhiroki): scheduler::NonMainThreadImpl and scheduler::
  // WorkerThreadScheduler are not in scheduler/public, then using them is a
  // layer violation. Fix this.
  auto& worker_thread = static_cast<scheduler::NonMainThreadImpl&>(
      GetWorkerBackingThread().BackingThread());
  worker_scheduler_ = scheduler::WorkerScheduler::CreateWorkerScheduler(
      static_cast<scheduler::WorkerThreadScheduler*>(
          worker_thread.GetNonMainThreadScheduler()),
      worker_thread.worker_scheduler_proxy());

  // Capture the worker task runners so that it's safe to access GetTaskRunner()
  // from any threads even after the worker scheduler is disposed of on the
  // worker thread. See also comments on GetTaskRunner().
  // We only capture task types that are actually used. When you want to use a
  // new task type, add it here.
  static constexpr TaskType kAvailableTaskTypes[] = {
      TaskType::kBackgroundFetch,
      TaskType::kCanvasBlobSerialization,
      TaskType::kDatabaseAccess,
      TaskType::kDOMManipulation,
      TaskType::kFileReading,
      TaskType::kFontLoading,
      TaskType::kInternalDefault,
      TaskType::kInternalInspector,
      TaskType::kInternalLoading,
      TaskType::kInternalMedia,
      TaskType::kInternalMediaRealTime,
      TaskType::kInternalTest,
      TaskType::kInternalWebCrypto,
      TaskType::kJavascriptTimerImmediate,
      TaskType::kJavascriptTimerDelayedLowNesting,
      TaskType::kJavascriptTimerDelayedHighNesting,
      TaskType::kMediaElementEvent,
      TaskType::kMachineLearning,
      TaskType::kMicrotask,
      TaskType::kMiscPlatformAPI,
      TaskType::kNetworking,
      TaskType::kNetworkingUnfreezable,
      TaskType::kPerformanceTimeline,
      TaskType::kPermission,
      TaskType::kPostedMessage,
      TaskType::kRemoteEvent,
      TaskType::kStorage,
      TaskType::kUserInteraction,
      TaskType::kWakeLock,
      TaskType::kWebGL,
      TaskType::kWebGPU,
      TaskType::kWebLocks,
      TaskType::kWebSocket,
      TaskType::kWorkerAnimation};
  worker_task_runners_.ReserveCapacityForSize(std::size(kAvailableTaskTypes));
  for (auto type : kAvailableTaskTypes) {
    auto task_runner = worker_scheduler_->GetTaskRunner(type);
    auto result = worker_task_runners_.insert(type, std::move(task_runner));
    DCHECK(result.is_new_entry);
  }

  waitable_event->Signal();
}

void WorkerThread::InitializeOnWorkerThread(
    std::unique_ptr<GlobalScopeCreationParams> global_scope_creation_params,
    const std::optional<WorkerBackingThreadStartupData>& thread_startup_data,
    std::unique_ptr<WorkerDevToolsParams> devtools_params) {
  base::ElapsedTimer timer;
  DCHECK(IsCurrentThread());
  backing_thread_weak_factory_.emplace(this);
  worker_reporting_proxy_.WillInitializeWorkerContext();
  {
    TRACE_EVENT0("blink.worker", "WorkerThread::InitializeWorkerContext");
    base::AutoLock locker(lock_);
    DCHECK_EQ(ThreadState::kNotStarted, thread_state_);

    if (IsOwningBackingThread()) {
      global_scope_creation_params->is_default_world_of_isolate = true;
      DCHECK(thread_startup_data.has_value());
      GetWorkerBackingThread().InitializeOnBackingThread(*thread_startup_data);
    } else {
      DCHECK(!thread_startup_data.has_value());
    }
    GetWorkerBackingThread().BackingThread().AddTaskObserver(this);

    // TODO(crbug.com/866666): Ideally this URL should be the response URL of
    // the worker top-level script, while currently can be the request URL
    // for off-the-main-thread top-level script fetch cases.
    const KURL url_for_debugger = global_scope_creation_params->script_url;

    console_message_storage_ = MakeGarbageCollected<ConsoleMessageStorage>();
    // Record this only for the DedicatedWorker.
    if (global_scope_creation_params->dedicated_worker_start_time.has_value()) {
      base::UmaHistogramTimes(
          "Worker.TopLevelScript.Initialization2GlobalScopeCreation",
          timer.Elapsed());
    }
    global_scope_ =
        CreateWorkerGlobalScope(std::move(global_scope_creation_params));
    worker_scheduler_->InitializeOnWorkerThread(global_scope_);
    worker_reporting_proxy_.DidCreateWorkerGlobalScope(GlobalScope());

    worker_inspector_controller_ = WorkerInspectorController::Create(
        this, url_for_debugger, inspector_task_runner_,
        std::move(devtools_params));

    // Since context initialization below may fail, we should notify debugger
    // about the new worker thread separately, so that it can resolve it by id
    // at any moment.
    if (WorkerThreadDebugger* debugger =
            WorkerThreadDebugger::From(GetIsolate()))
      debugger->WorkerThreadCreated(this);

    GlobalScope()->ScriptController()->Initialize(url_for_debugger);
    GlobalScope()->WillBeginLoading();
    v8::HandleScope handle_scope(GetIsolate());
    Platform::Current()->WorkerContextCreated(
        GlobalScope()->ScriptController()->GetContext());

    inspector_task_runner_->InitIsolate(GetIsolate());
    SetThreadState(ThreadState::kRunning);
  }

  if (CheckRequestedToTerminate()) {
    // Stop further worker tasks from running after this point. WorkerThread
    // was requested to terminate before initialization.
    // PerformShutdownOnWorkerThread() will be called soon.
    PrepareForShutdownOnWorkerThread();
    return;
  }

  {
    base::AutoLock locker(ThreadSetLock());
    DCHECK(InitializingWorkerThreads().Contains(this));
    DCHECK(!WorkerThreads().Contains(this));
    InitializingWorkerThreads().erase(this);
    WorkerThreads().insert(this);
  }

  // It is important that no code is run on the Isolate between
  // initializing InspectorTaskRunner and pausing on start.
  // Otherwise, InspectorTaskRunner might interrupt isolate execution
  // from another thread and try to resume "pause on start" before
  // we even paused.
  worker_inspector_controller_->WaitForDebuggerIfNeeded();
  // Note the above call runs nested message loop which may result in
  // worker thread being torn down by request from the parent thread,
  // while waiting for debugger.
}

void WorkerThread::EvaluateClassicScriptOnWorkerThread(
    const KURL& script_url,
    String source_code,
    std::unique_ptr<Vector<uint8_t>> cached_meta_data,
    const v8_inspector::V8StackTraceId& stack_id) {
  WorkerGlobalScope* global_scope = To<WorkerGlobalScope>(GlobalScope());
  CHECK(global_scope);
  global_scope->EvaluateClassicScript(script_url, std::move(source_code),
                                      std::move(cached_meta_data), stack_id);
}

void WorkerThread::FetchAndRunClassicScriptOnWorkerThread(
    const KURL& script_url,
    std::unique_ptr<WorkerMainScriptLoadParameters>
        worker_main_script_load_params,
    std::unique_ptr<WebPolicyContainer> policy_container,
    std::unique_ptr<CrossThreadFetchClientSettingsObjectData>
        outside_settings_object,
    WorkerResourceTimingNotifier* outside_resource_timing_notifier,
    const v8_inspector::V8StackTraceId& stack_id) {
  if (!outside_resource_timing_notifier) {
    outside_resource_timing_notifier =
        MakeGarbageCollected<NullWorkerResourceTimingNotifier>();
  }

  To<WorkerGlobalScope>(GlobalScope())
      ->FetchAndRunClassicScript(
          script_url, std::move(worker_main_script_load_params),
          PolicyContainer::CreateFromWebPolicyContainer(
              std::move(policy_container)),
          *MakeGarbageCollected<FetchClientSettingsObjectSnapshot>(
              std::move(outside_settings_object)),
          *outside_resource_timing_notifier, stack_id);
}

void WorkerThread::FetchAndRunModuleScriptOnWorkerThread(
    const KURL& script_url,
    std::unique_ptr<WorkerMainScriptLoadParameters>
        worker_main_script_load_params,
    std::unique_ptr<WebPolicyContainer> policy_container,
    std::unique_ptr<CrossThreadFetchClientSettingsObjectData>
        outside_settings_object,
    WorkerResourceTimingNotifier* outside_resource_timing_notifier,
    network::mojom::CredentialsMode credentials_mode,
    bool reject_coep_unsafe_none) {
  if (!outside_resource_timing_notifier) {
    outside_resource_timing_notifier =
        MakeGarbageCollected<NullWorkerResourceTimingNotifier>();
  }
  // Worklets have a different code path to import module scripts.
  // TODO(nhiroki): Consider excluding this code path from WorkerThread like
  // Worklets.
  To<WorkerGlobalScope>(GlobalScope())
      ->FetchAndRunModuleScript(
          script_url, std::move(worker_main_script_load_params),
          PolicyContainer::CreateFromWebPolicyContainer(
              std::move(policy_container)),
          *MakeGarbageCollected<FetchClientSettingsObjectSnapshot>(
              std::move(outside_settings_object)),
          *outside_resource_timing_notifier, credentials_mode,
          RejectCoepUnsafeNone(reject_coep_unsafe_none));
}

void WorkerThread::PrepareForShutdownOnWorkerThread() {
  DCHECK(IsCurrentThread());
  {
    base::AutoLock locker(lock_);
    if (thread_state_ == ThreadState::kReadyToShutdown)
      return;
    SetThreadState(ThreadState::kReadyToShutdown);
  }

  backing_thread_weak_factory_ = std::nullopt;
  if (pause_or_freeze_count_ > 0) {
    DCHECK(nested_runner_);
    pause_or_freeze_count_ = 0;
    nested_runner_->QuitNow();
  }
  pause_handle_.reset();

  if (WorkerThreadDebugger* debugger = WorkerThreadDebugger::From(GetIsolate()))
    debugger->WorkerThreadDestroyed(this);

  GetWorkerReportingProxy().WillDestroyWorkerGlobalScope();

  probe::AllAsyncTasksCanceled(GlobalScope());

  // This will eventually call the |child_threads_|'s Terminate() through
  // ContextLifecycleObserver::ContextDestroyed(), because the nested workers
  // are observer of the |GlobalScope()| (see the DedicatedWorker class) and
  // they initiate thread termination on destruction of the parent context.
  GlobalScope()->NotifyContextDestroyed();

  worker_scheduler_->Dispose();

  // No V8 microtasks should get executed after shutdown is requested.
  GetWorkerBackingThread().BackingThread().RemoveTaskObserver(this);
}

void WorkerThread::PerformShutdownOnWorkerThread() {
  DCHECK(IsCurrentThread());
  {
    base::AutoLock locker(lock_);
    DCHECK(requested_to_terminate_);
    DCHECK_EQ(ThreadState::kReadyToShutdown, thread_state_);
    if (exit_code_ == ExitCode::kNotTerminated)
      SetExitCode(ExitCode::kGracefullyTerminated);
  }

  // When child workers are present, wait for them to shutdown before shutting
  // down this thread. ChildThreadTerminatedOnWorkerThread() is responsible
  // for completing shutdown on the worker thread after the last child shuts
  // down.
  if (!child_threads_.empty())
    return;

  inspector_task_runner_->Dispose();
  if (worker_inspector_controller_) {
    worker_inspector_controller_->Dispose();
    worker_inspector_controller_.Clear();
  }

  GlobalScope()->Dispose();
  global_scope_ = nullptr;

  console_message_storage_.Clear();
  inspector_issue_storage_.Clear();

  if (IsOwningBackingThread())
    GetWorkerBackingThread().ShutdownOnBackingThread();
  // We must not touch GetWorkerBackingThread() from now on.

  // Keep the reference to the shutdown event in a local variable so that the
  // worker thread can signal it even after calling DidTerminateWorkerThread(),
  // which may destroy |this|.
  scoped_refptr<RefCountedWaitableEvent> shutdown_event = shutdown_event_;

  // Notify the proxy that the WorkerOrWorkletGlobalScope has been disposed
  // of. This can free this thread object, hence it must not be touched
  // afterwards.
  GetWorkerReportingProxy().DidTerminateWorkerThread();

  // This should be signaled at the end because this may induce the main thread
  // to clear the worker backing thread and stop thread execution in the system
  // level.
  shutdown_event->Signal();
}

void WorkerThread::SetThreadState(ThreadState next_thread_state) {
  switch (next_thread_state) {
    case ThreadState::kNotStarted:
      NOTREACHED_IN_MIGRATION();
      return;
    case ThreadState::kRunning:
      DCHECK_EQ(ThreadState::kNotStarted, thread_state_);
      thread_state_ = next_thread_state;
      return;
    case ThreadState::kReadyToShutdown:
      DCHECK_EQ(ThreadState::kRunning, thread_state_);
      thread_state_ = next_thread_state;
      return;
  }
}

void WorkerThread::SetExitCode(ExitCode exit_code) {
  DCHECK_EQ(ExitCode::kNotTerminated, exit_code_);
  exit_code_ = exit_code;
}

bool WorkerThread::CheckRequestedToTerminate() {
  base::AutoLock locker(lock_);
  return requested_to_terminate_;
}

void WorkerThread::PauseOrFreeze(mojom::blink::FrameLifecycleState state,
                                 bool is_in_back_forward_cache) {
  DCHECK(!is_in_back_forward_cache ||
         state == mojom::blink::FrameLifecycleState::kFrozen);

  if (IsCurrentThread()) {
    PauseOrFreezeOnWorkerThread(state, is_in_back_forward_cache);
  } else {
    // We send a V8 interrupt to break active JS script execution because
    // workers might not yield. Likewise we might not be in JS and the
    // interrupt might not fire right away, so we post a task as well.
    // Use a token to mitigate both the interrupt and post task firing.
    base::AutoLock locker(lock_);

    InterruptData* interrupt_data =
        new InterruptData(this, state, is_in_back_forward_cache);
    pending_interrupts_.insert(std::unique_ptr<InterruptData>(interrupt_data));

    if (auto* isolate = GetIsolate()) {
      isolate->RequestInterrupt(&PauseOrFreezeInsideV8InterruptOnWorkerThread,
                                interrupt_data);
    }
    PostCrossThreadTask(
        *GetWorkerBackingThread().BackingThread().GetTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(
            &WorkerThread::PauseOrFreezeInsidePostTaskOnWorkerThread,
            CrossThreadUnretained(interrupt_data)));
  }
}

void WorkerThread::PauseOrFreezeOnWorkerThread(
    mojom::blink::FrameLifecycleState state,
    bool is_in_back_forward_cache) {
  DCHECK(IsCurrentThread());
  DCHECK(state == mojom::blink::FrameLifecycleState::kFrozen ||
         state == mojom::blink::FrameLifecycleState::kPaused);
  DCHECK(!is_in_back_forward_cache ||
         state == mojom::blink::FrameLifecycleState::kFrozen);

  // Ensure we aren't trying to pause a worker that should be terminating.
  {
    base::AutoLock locker(lock_);
    if (thread_state_ != ThreadState::kRunning)
      return;
  }

  pause_or_freeze_count_++;
  GlobalScope()->SetIsInBackForwardCache(is_in_back_forward_cache);
  GlobalScope()->SetLifecycleState(state);
  GlobalScope()->SetDefersLoadingForResourceFetchers(
      GlobalScope()->GetLoaderFreezeMode());

  // If already paused return early.
  if (pause_or_freeze_count_ > 1)
    return;

  pause_handle_ = GetScheduler()->Pause();
  {
    // Since the nested message loop runner needs to be created and destroyed on
    // the same thread we allocate and destroy a new message loop runner each
    // time we pause or freeze. The AutoReset allows a raw ptr to be stored in
    // the worker thread such that the resume/terminate can quit this runner.
    std::unique_ptr<Platform::NestedMessageLoopRunner> nested_runner =
        Platform::Current()->CreateNestedMessageLoopRunner();
    auto weak_this = backing_thread_weak_factory_->GetWeakPtr();
    nested_runner_ = nested_runner.get();
    nested_runner->Run();

    // Careful `this` may be destroyed.
    if (!weak_this) {
      return;
    }
    nested_runner_ = nullptr;
  }
  GlobalScope()->SetDefersLoadingForResourceFetchers(LoaderFreezeMode::kNone);
  GlobalScope()->SetIsInBackForwardCache(false);
  GlobalScope()->SetLifecycleState(mojom::blink::FrameLifecycleState::kRunning);
  pause_handle_.reset();
}

void WorkerThread::ResumeOnWorkerThread() {
  DCHECK(IsCurrentThread());
  if (pause_or_freeze_count_ > 0) {
    DCHECK(nested_runner_);
    pause_or_freeze_count_--;
    if (pause_or_freeze_count_ == 0)
      nested_runner_->QuitNow();
  }
}

void WorkerThread::PauseOrFreezeWithInterruptDataOnWorkerThread(
    InterruptData* interrupt_data) {
  DCHECK(IsCurrentThread());
  bool should_execute = false;
  mojom::blink::FrameLifecycleState state;
  {
    base::AutoLock locker(lock_);
    state = interrupt_data->state();
    // If both the V8 interrupt and PostTask have executed we can remove
    // the matching InterruptData from the |pending_interrupts_| as it is
    // no longer used.
    if (interrupt_data->ShouldRemoveFromList()) {
      auto iter = pending_interrupts_.begin();
      while (iter != pending_interrupts_.end()) {
        if (iter->get() == interrupt_data) {
          pending_interrupts_.erase(iter);
          break;
        }
        ++iter;
      }
    } else {
      should_execute = true;
    }
  }

  if (should_execute) {
    PauseOrFreezeOnWorkerThread(state,
                                interrupt_data->is_in_back_forward_cache());
  }
}

void WorkerThread::PauseOrFreezeInsideV8InterruptOnWorkerThread(v8::Isolate*,
                                                                void* data) {
  InterruptData* interrupt_data = static_cast<InterruptData*>(data);
  interrupt_data->MarkInterruptCalled();
  interrupt_data->worker_thread()->PauseOrFreezeWithInterruptDataOnWorkerThread(
      interrupt_data);
}

void WorkerThread::PauseOrFreezeInsidePostTaskOnWorkerThread(
    InterruptData* interrupt_data) {
  interrupt_data->MarkPostTaskCalled();
  interrupt_data->worker_thread()->PauseOrFreezeWithInterruptDataOnWorkerThread(
      interrupt_data);
}

}  // namespace blink
