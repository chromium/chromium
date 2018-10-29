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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKER_THREAD_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKER_THREAD_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_annotations.h"
#include "base/unguessable_token.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/blink/public/platform/web_thread_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/workers/parent_execution_context_task_runners.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread_startup_data.h"
#include "third_party/blink/renderer/core/workers/worker_inspector_proxy.h"
#include "third_party/blink/renderer/platform/loader/fetch/access_control_status.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_scheduler.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "v8/include/v8.h"

namespace blink {

class ConsoleMessageStorage;
class FetchClientSettingsObjectSnapshot;
class InspectorTaskRunner;
class InstalledScriptsManager;
class WorkerBackingThread;
class WorkerInspectorController;
class WorkerOrWorkletGlobalScope;
class WorkerReportingProxy;
struct CrossThreadFetchClientSettingsObjectData;
struct GlobalScopeCreationParams;

// WorkerThread is a kind of WorkerBackingThread client. Each worker mechanism
// can access the lower thread infrastructure via an implementation of this
// abstract class. Multiple WorkerThreads may share one WorkerBackingThread for
// worklets.
//
// WorkerThread start and termination must be initiated on the main thread and
// an actual task is executed on the worker thread.
//
// When termination starts, (debugger) tasks on WorkerThread are handled as
// follows:
//  - A running task may finish unless a forcible termination task interrupts.
//    If the running task is for debugger, it's guaranteed to finish without
//    any interruptions.
//  - Queued tasks never run.
class CORE_EXPORT WorkerThread : public Thread::TaskObserver {
 public:
  // Represents how this thread is terminated. Used for UMA. Append only.
  enum class ExitCode {
    kNotTerminated,
    kGracefullyTerminated,
    kSyncForciblyTerminated,
    kAsyncForciblyTerminated,
    kLastEnum,
  };

  ~WorkerThread() override;

  // Starts the underlying thread and creates the global scope. Called on the
  // main thread.
  // Startup data for WorkerBackingThread is base::nullopt if |this| doesn't own
  // the underlying WorkerBackingThread.
  // TODO(nhiroki): We could separate WorkerBackingThread initialization from
  // GlobalScope initialization sequence, that is, InitializeOnWorkerThread().
  // After that, we could remove this startup data for WorkerBackingThread.
  // (https://crbug.com/710364)
  void Start(std::unique_ptr<GlobalScopeCreationParams>,
             const base::Optional<WorkerBackingThreadStartupData>&,
             WorkerInspectorProxy::PauseOnWorkerStart,
             ParentExecutionContextTaskRunners*);

  // Posts a task to evaluate a top-level classic script on the worker thread.
  // Called on the main thread after Start().
  void EvaluateClassicScript(const KURL& script_url,
                             AccessControlStatus access_control_status,
                             const String& source_code,
                             std::unique_ptr<Vector<char>> cached_meta_data,
                             const v8_inspector::V8StackTraceId& stack_id);

  // Posts a task to import a top-level module script on the worker thread.
  // Called on the main thread after start().
  void ImportModuleScript(
      const KURL& script_url,
      FetchClientSettingsObjectSnapshot* outside_settings_object,
      network::mojom::FetchCredentialsMode);

  // Posts a task to the worker thread to close the global scope and terminate
  // the underlying thread. This task may be blocked by JavaScript execution on
  // the worker thread, so this function also forcibly terminates JavaScript
  // execution after a certain grace period.
  void Terminate() LOCKS_EXCLUDED(mutex_);

  // Terminates the worker thread. Subclasses of WorkerThread can override this
  // to do cleanup. The default behavior is to call Terminate() and
  // synchronously call EnsureScriptExecutionTerminates() to ensure the thread
  // is quickly terminated. Called on the main thread.
  virtual void TerminateForTesting();

  // Called on the main thread for the leak detector. Forcibly terminates the
  // script execution and waits by *blocking* the calling thread until the
  // workers are shut down. Please be careful when using this function, because
  // after the synchronous termination any V8 APIs may suddenly start to return
  // empty handles and it may cause crashes.
  // WARNING: This is not safe if a nested worker is running.
  static void TerminateAllWorkersForTesting();

  // Thread::TaskObserver.
  void WillProcessTask(const base::PendingTask&) override;
  void DidProcessTask(const base::PendingTask&) override;

  virtual WorkerBackingThread& GetWorkerBackingThread() = 0;
  virtual void ClearWorkerBackingThread() = 0;
  ConsoleMessageStorage* GetConsoleMessageStorage() const {
    return console_message_storage_.Get();
  }
  v8::Isolate* GetIsolate();

  bool IsCurrentThread();

  WorkerReportingProxy& GetWorkerReportingProxy() const {
    return worker_reporting_proxy_;
  }

  // Only constructible on the main thread.
  class CORE_EXPORT ScopedDebuggerTask {
    STACK_ALLOCATED();

   public:
    explicit ScopedDebuggerTask(WorkerThread*);
    ~ScopedDebuggerTask();

   private:
    WorkerThread* thread_;
    DISALLOW_COPY_AND_ASSIGN(ScopedDebuggerTask);
  };
  InspectorTaskRunner* GetInspectorTaskRunner();

  // Callable on both the main thread and the worker thread.
  const base::UnguessableToken& GetDevToolsWorkerToken() const {
    return devtools_worker_token_;
  }

  // Can be called only on the worker thread, WorkerOrWorkletGlobalScope
  // and WorkerInspectorController are not thread safe.
  WorkerOrWorkletGlobalScope* GlobalScope();
  WorkerInspectorController* GetWorkerInspectorController();

  // Number of active worker threads.
  static unsigned WorkerThreadCount();

  // Runs |function| with |parameters| on each worker thread, and
  // adds the current WorkerThread* as the first parameter |function|.
  template <typename FunctionType, typename... Parameters>
  static void CallOnAllWorkerThreads(FunctionType function,
                                     Parameters&&... parameters) {
    MutexLocker lock(ThreadSetMutex());
    for (WorkerThread* thread : WorkerThreads()) {
      PostCrossThreadTask(
          *thread->GetTaskRunner(TaskType::kInternalWorker), FROM_HERE,
          CrossThreadBind(function, WTF::CrossThreadUnretained(thread),
                          parameters...));
    }
  }

  int GetWorkerThreadId() const { return worker_thread_id_; }

  PlatformThreadId GetPlatformThreadId();

  bool IsForciblyTerminated() LOCKS_EXCLUDED(mutex_);

  void WaitForShutdownForTesting();
  ExitCode GetExitCodeForTesting() LOCKS_EXCLUDED(mutex_);

  ParentExecutionContextTaskRunners* GetParentExecutionContextTaskRunners()
      const {
    return parent_execution_context_task_runners_.Get();
  }

  // For ServiceWorkerScriptStreaming. Returns nullptr otherwise.
  virtual InstalledScriptsManager* GetInstalledScriptsManager() {
    return nullptr;
  }

  scheduler::WorkerScheduler* GetScheduler();

  // Returns a task runner bound to the per-global-scope scheduler's task queue.
  // You don't have to care about the lifetime of the associated global scope
  // and underlying thread. After the global scope is destroyed, queued tasks
  // are discarded and PostTask on the returned task runner just fails. This
  // function can be called on both the main thread and the worker thread.
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(TaskType type) {
    return worker_scheduler_->GetTaskRunner(type);
  }

  void ChildThreadStartedOnWorkerThread(WorkerThread*);
  void ChildThreadTerminatedOnWorkerThread(WorkerThread*);

 protected:
  explicit WorkerThread(WorkerReportingProxy&);

  virtual WebThreadType GetThreadType() const = 0;

  // Official moment of creation of worker: when the worker thread is created.
  // (https://w3c.github.io/hr-time/#time-origin)
  const base::TimeTicks time_origin_;

 private:
  friend class WorkerThreadTest;
  FRIEND_TEST_ALL_PREFIXES(WorkerThreadTest, ShouldTerminateScriptExecution);
  FRIEND_TEST_ALL_PREFIXES(
      WorkerThreadTest,
      Terminate_WhileDebuggerTaskIsRunningOnInitialization);
  FRIEND_TEST_ALL_PREFIXES(WorkerThreadTest,
                           Terminate_WhileDebuggerTaskIsRunning);

  static HashSet<WorkerThread*>& WorkerThreads();
  static Mutex& ThreadSetMutex();

  // Represents the state of this worker thread.
  enum class ThreadState {
    kNotStarted,
    kRunning,
    kReadyToShutdown,
  };

  // Factory method for creating a new worker context for the thread.
  // Called on the worker thread.
  virtual WorkerOrWorkletGlobalScope* CreateWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams>) = 0;

  // Returns true when this WorkerThread owns the associated
  // WorkerBackingThread exclusively. If this function returns true, the
  // WorkerThread initializes / shutdowns the backing thread. Otherwise
  // the backing thread should be initialized / shutdown properly out of this
  // class.
  virtual bool IsOwningBackingThread() const { return true; }

  // Posts a delayed task to forcibly terminate script execution in case the
  // normal shutdown sequence does not start within a certain time period.
  void ScheduleToTerminateScriptExecution();

  // Returns true if we should synchronously terminate the script execution so
  // that a shutdown task can be handled by the thread event loop.
  bool ShouldTerminateScriptExecution() EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Terminates worker script execution if the worker thread is running and not
  // already shutting down. Does not terminate if a debugger task is running,
  // because the debugger task is guaranteed to finish and it heavily uses V8
  // API calls which would crash after forcible script termination. Called on
  // the main thread.
  void EnsureScriptExecutionTerminates(ExitCode) LOCKS_EXCLUDED(mutex_);

  // These are called in this order during worker thread startup.
  void InitializeSchedulerOnWorkerThread(base::WaitableEvent*);
  void InitializeOnWorkerThread(
      std::unique_ptr<GlobalScopeCreationParams>,
      const base::Optional<WorkerBackingThreadStartupData>&,
      WorkerInspectorProxy::PauseOnWorkerStart) LOCKS_EXCLUDED(mutex_);

  void EvaluateClassicScriptOnWorkerThread(
      const KURL& script_url,
      AccessControlStatus access_control_status,
      String source_code,
      std::unique_ptr<Vector<char>> cached_meta_data,
      const v8_inspector::V8StackTraceId& stack_id);
  void ImportModuleScriptOnWorkerThread(
      const KURL& script_url,
      std::unique_ptr<CrossThreadFetchClientSettingsObjectData>
          outside_settings_object,
      network::mojom::FetchCredentialsMode);

  // These are called in this order during worker thread termination.
  void PrepareForShutdownOnWorkerThread() LOCKS_EXCLUDED(mutex_);
  void PerformShutdownOnWorkerThread() LOCKS_EXCLUDED(mutex_);

  void SetThreadState(ThreadState) EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void SetExitCode(ExitCode) EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool CheckRequestedToTerminate() LOCKS_EXCLUDED(mutex_);

  // A unique identifier among all WorkerThreads.
  const int worker_thread_id_;

  // Set on the main thread.
  bool requested_to_terminate_ GUARDED_BY(mutex_) = false;

  ThreadState thread_state_ GUARDED_BY(mutex_) = ThreadState::kNotStarted;
  ExitCode exit_code_ GUARDED_BY(mutex_) = ExitCode::kNotTerminated;

  TimeDelta forcible_termination_delay_;

  scoped_refptr<InspectorTaskRunner> inspector_task_runner_;
  const base::UnguessableToken devtools_worker_token_;
  int debugger_task_counter_ GUARDED_BY(mutex_) = 0;

  WorkerReportingProxy& worker_reporting_proxy_;

  CrossThreadPersistent<ParentExecutionContextTaskRunners>
      parent_execution_context_task_runners_;

  // Tasks managed by this scheduler are canceled when the global scope is
  // closed.
  std::unique_ptr<scheduler::WorkerScheduler> worker_scheduler_;

  // This lock protects shared states between the main thread and the worker
  // thread. See thread-safety annotations (e.g., GUARDED_BY) in this header
  // file.
  Mutex mutex_;

  CrossThreadPersistent<ConsoleMessageStorage> console_message_storage_;
  CrossThreadPersistent<WorkerOrWorkletGlobalScope> global_scope_;
  CrossThreadPersistent<WorkerInspectorController> worker_inspector_controller_;

  // Signaled when the thread completes termination on the worker thread. Only
  // the parent context thread should wait on this event after calling
  // Terminate().
  class RefCountedWaitableEvent;
  scoped_refptr<RefCountedWaitableEvent> shutdown_event_;

  // Used to cancel a scheduled forcible termination task. See
  // mayForciblyTerminateExecution() for details.
  TaskHandle forcible_termination_task_handle_;

  HashSet<WorkerThread*> child_threads_;

  THREAD_CHECKER(parent_thread_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKER_THREAD_H_
