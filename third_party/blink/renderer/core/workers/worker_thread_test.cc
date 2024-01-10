// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/worker_thread.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/v8_cache_options.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/inspector_task_runner.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"
#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_thread_test_helper.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

using testing::_;
using testing::AtMost;

namespace blink {

using ExitCode = WorkerThread::ExitCode;

namespace {

// Used as a debugger task. Waits for a signal from the main thread.
void WaitForSignalTask(WorkerThread* worker_thread,
                       base::WaitableEvent* waitable_event,
                       CrossThreadOnceClosure quit_closure) {
  EXPECT_TRUE(worker_thread->IsCurrentThread());

  worker_thread->DebuggerTaskStarted();
  // Notify the main thread that the debugger task is waiting for the signal.
  PostCrossThreadTask(*worker_thread->GetParentTaskRunnerForTesting(),
                      FROM_HERE, CrossThreadBindOnce(std::move(quit_closure)));
  waitable_event->Wait();
  worker_thread->DebuggerTaskFinished();
}

void TerminateParentOfNestedWorker(WorkerThread* parent_thread,
                                   base::WaitableEvent* waitable_event) {
  EXPECT_TRUE(IsMainThread());
  parent_thread->Terminate();
  waitable_event->Signal();
}

void PauseExecution(v8::Isolate* isolate, void* data) {
  WorkerThread* worker_thread = static_cast<WorkerThread*>(data);
  WorkerThreadDebugger* debugger =
      WorkerThreadDebugger::From(worker_thread->GetIsolate());
  debugger->PauseWorkerOnStart(worker_thread);
}

// This helper managers a child worker thread and a reporting proxy
// and ensures they stay alive for the duration of the test. The struct
// is created on the main thread, but its members are created and
// destroyed on the parent worker thread.
struct NestedWorkerHelper {
 public:
  NestedWorkerHelper() = default;
  ~NestedWorkerHelper() = default;

  std::unique_ptr<MockWorkerReportingProxy> reporting_proxy;
  std::unique_ptr<WorkerThreadForTest> worker_thread;
};

void CreateNestedWorkerThenTerminateParent(
    WorkerThread* parent_thread,
    NestedWorkerHelper* nested_worker_helper,
    CrossThreadOnceClosure quit_closure) {
  EXPECT_TRUE(parent_thread->IsCurrentThread());

  nested_worker_helper->reporting_proxy =
      std::make_unique<MockWorkerReportingProxy>();
  EXPECT_CALL(*nested_worker_helper->reporting_proxy,
              DidCreateWorkerGlobalScope(_))
      .Times(1);
  EXPECT_CALL(*nested_worker_helper->reporting_proxy, WillEvaluateScriptMock())
      .Times(1);
  EXPECT_CALL(*nested_worker_helper->reporting_proxy,
              DidEvaluateTopLevelScript(true))
      .Times(1);
  EXPECT_CALL(*nested_worker_helper->reporting_proxy,
              WillDestroyWorkerGlobalScope())
      .Times(1);
  EXPECT_CALL(*nested_worker_helper->reporting_proxy,
              DidTerminateWorkerThread())
      .Times(1);

  nested_worker_helper->worker_thread = std::make_unique<WorkerThreadForTest>(
      *nested_worker_helper->reporting_proxy);
  nested_worker_helper->worker_thread->StartWithSourceCode(
      SecurityOrigin::Create(KURL("http://fake.url/")).get(),
      "//fake source code");
  nested_worker_helper->worker_thread->WaitForInit();

  // Ask the main threat to terminate this parent thread.
  base::WaitableEvent child_waitable;
  PostCrossThreadTask(
      *parent_thread->GetParentTaskRunnerForTesting(), FROM_HERE,
      CrossThreadBindOnce(&TerminateParentOfNestedWorker,
                          CrossThreadUnretained(parent_thread),
                          CrossThreadUnretained(&child_waitable)));
  child_waitable.Wait();
  EXPECT_EQ(ExitCode::kNotTerminated, parent_thread->GetExitCodeForTesting());

  parent_thread->ChildThreadStartedOnWorkerThread(
      nested_worker_helper->worker_thread.get());
  PostCrossThreadTask(*parent_thread->GetParentTaskRunnerForTesting(),
                      FROM_HERE, CrossThreadBindOnce(std::move(quit_closure)));
}

void VerifyParentAndChildAreTerminated(WorkerThread* parent_thread,
                                       NestedWorkerHelper* nested_worker_helper,
                                       base::WaitableEvent* waitable_event) {
  EXPECT_TRUE(parent_thread->IsCurrentThread());
  EXPECT_EQ(ExitCode::kGracefullyTerminated,
            parent_thread->GetExitCodeForTesting());
  EXPECT_NE(nullptr, parent_thread->GlobalScope());

  parent_thread->ChildThreadTerminatedOnWorkerThread(
      nested_worker_helper->worker_thread.get());
  EXPECT_EQ(nullptr, parent_thread->GlobalScope());

  nested_worker_helper->worker_thread = nullptr;
  nested_worker_helper->reporting_proxy = nullptr;
  waitable_event->Signal();
}

}  // namespace

class WorkerThreadTest : public testing::Test {
 public:
  WorkerThreadTest() = default;

  void SetUp() override {
    reporting_proxy_ = std::make_unique<MockWorkerReportingProxy>();
    security_origin_ = SecurityOrigin::Create(KURL("http://fake.url/"));
    worker_thread_ = std::make_unique<WorkerThreadForTest>(*reporting_proxy_);
  }

  void TearDown() override {}

  void Start() {
    worker_thread_->StartWithSourceCode(security_origin_.get(),
                                        "//fake source code");
  }

  void StartWithSourceCodeNotToFinish() {
    // Use a JavaScript source code that makes an infinite loop so that we
    // can catch some kind of issues as a timeout.
    worker_thread_->StartWithSourceCode(security_origin_.get(),
                                        "while(true) {}");
  }

  void SetForcibleTerminationDelay(base::TimeDelta forcible_termination_delay) {
    worker_thread_->forcible_termination_delay_ = forcible_termination_delay;
  }

  bool IsForcibleTerminationTaskScheduled() {
    return worker_thread_->forcible_termination_task_handle_.IsActive();
  }

 protected:
  void ExpectReportingCalls() {
    EXPECT_CALL(*reporting_proxy_, DidCreateWorkerGlobalScope(_)).Times(1);
    EXPECT_CALL(*reporting_proxy_, WillEvaluateScriptMock()).Times(1);
    EXPECT_CALL(*reporting_proxy_, DidEvaluateTopLevelScript(true)).Times(1);
    EXPECT_CALL(*reporting_proxy_, WillDestroyWorkerGlobalScope()).Times(1);
    EXPECT_CALL(*reporting_proxy_, DidTerminateWorkerThread()).Times(1);
  }

  void ExpectReportingCallsForWorkerPossiblyTerminatedBeforeInitialization() {
    EXPECT_CALL(*reporting_proxy_, DidCreateWorkerGlobalScope(_)).Times(1);
    EXPECT_CALL(*reporting_proxy_, WillEvaluateScriptMock()).Times(AtMost(1));
    EXPECT_CALL(*reporting_proxy_, DidEvaluateTopLevelScript(_))
        .Times(AtMost(1));
    EXPECT_CALL(*reporting_proxy_, WillDestroyWorkerGlobalScope())
        .Times(AtMost(1));
    EXPECT_CALL(*reporting_proxy_, DidTerminateWorkerThread()).Times(1);
  }

  void ExpectReportingCallsForWorkerForciblyTerminated() {
    EXPECT_CALL(*reporting_proxy_, DidCreateWorkerGlobalScope(_)).Times(1);
    EXPECT_CALL(*reporting_proxy_, WillEvaluateScriptMock()).Times(1);
    EXPECT_CALL(*reporting_proxy_, DidEvaluateTopLevelScript(false)).Times(1);
    EXPECT_CALL(*reporting_proxy_, WillDestroyWorkerGlobalScope()).Times(1);
    EXPECT_CALL(*reporting_proxy_, DidTerminateWorkerThread()).Times(1);
  }

  ExitCode GetExitCode() { return worker_thread_->GetExitCodeForTesting(); }

  test::TaskEnvironment task_environment_;
  scoped_refptr<const SecurityOrigin> security_origin_;
  std::unique_ptr<MockWorkerReportingProxy> reporting_proxy_;
  std::unique_ptr<WorkerThreadForTest> worker_thread_;
};

TEST_F(WorkerThreadTest, ShouldTerminateScriptExecution) {
  using ThreadState = WorkerThread::ThreadState;

  worker_thread_->inspector_task_runner_ = InspectorTaskRunner::Create(nullptr);

  // SetExitCode() and ShouldTerminateScriptExecution() require the lock.
  base::AutoLock dummy_locker(worker_thread_->lock_);

  EXPECT_EQ(ThreadState::kNotStarted, worker_thread_->thread_state_);
  EXPECT_EQ(WorkerThread::TerminationState::kTerminationUnnecessary,
            worker_thread_->ShouldTerminateScriptExecution());

  worker_thread_->SetThreadState(ThreadState::kRunning);
  EXPECT_EQ(WorkerThread::TerminationState::kTerminate,
            worker_thread_->ShouldTerminateScriptExecution());

  worker_thread_->debugger_task_counter_ = 1;
  EXPECT_EQ(WorkerThread::TerminationState::kPostponeTerminate,
            worker_thread_->ShouldTerminateScriptExecution());
  worker_thread_->debugger_task_counter_ = 0;

  worker_thread_->SetThreadState(ThreadState::kReadyToShutdown);
  EXPECT_EQ(WorkerThread::TerminationState::kTerminate,
            worker_thread_->ShouldTerminateScriptExecution());

  worker_thread_->SetExitCode(ExitCode::kGracefullyTerminated);
  EXPECT_EQ(WorkerThread::TerminationState::kTerminationUnnecessary,
            worker_thread_->ShouldTerminateScriptExecution());
}

TEST_F(WorkerThreadTest, AsyncTerminate_OnIdle) {
  ExpectReportingCalls();
  Start();

  // Wait until the initialization completes and the worker thread becomes
  // idle.
  worker_thread_->WaitForInit();

  // The worker thread is not being blocked, so the worker thread should be
  // gracefully shut down.
  worker_thread_->Terminate();
  EXPECT_TRUE(IsForcibleTerminationTaskScheduled());
  worker_thread_->WaitForShutdownForTesting();
  EXPECT_EQ(ExitCode::kGracefullyTerminated, GetExitCode());
}

TEST_F(WorkerThreadTest, SyncTerminate_OnIdle) {
  ExpectReportingCalls();
  Start();

  // Wait until the initialization completes and the worker thread becomes
  // idle.
  worker_thread_->WaitForInit();

  worker_thread_->TerminateForTesting();
  worker_thread_->WaitForShutdownForTesting();

  // The worker thread may gracefully shut down before forcible termination
  // runs.
  ExitCode exit_code = GetExitCode();
  EXPECT_TRUE(ExitCode::kGracefullyTerminated == exit_code ||
              ExitCode::kSyncForciblyTerminated == exit_code);
}

TEST_F(WorkerThreadTest, AsyncTerminate_ImmediatelyAfterStart) {
  ExpectReportingCallsForWorkerPossiblyTerminatedBeforeInitialization();
  Start();

  // The worker thread is not being blocked, so the worker thread should be
  // gracefully shut down.
  worker_thread_->Terminate();
  worker_thread_->WaitForShutdownForTesting();
  EXPECT_EQ(ExitCode::kGracefullyTerminated, GetExitCode());
}

TEST_F(WorkerThreadTest, SyncTerminate_ImmediatelyAfterStart) {
  ExpectReportingCallsForWorkerPossiblyTerminatedBeforeInitialization();
  Start();

  // There are two possible cases depending on timing:
  // (1) If the thread hasn't been initialized on the worker thread yet,
  // TerminateForTesting() should wait for initialization and shut down the
  // thread immediately after that.
  // (2) If the thread has already been initialized on the worker thread,
  // TerminateForTesting() should synchronously forcibly terminates the worker
  // script execution.
  worker_thread_->TerminateForTesting();
  worker_thread_->WaitForShutdownForTesting();
  ExitCode exit_code = GetExitCode();
  EXPECT_TRUE(ExitCode::kGracefullyTerminated == exit_code ||
              ExitCode::kSyncForciblyTerminated == exit_code);
}

// TODO(crbug.com/1503519): The test is flaky on Linux TSan
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_AsyncTerminate_WhileTaskIsRunning \
  DISABLED_AsyncTerminate_WhileTaskIsRunning
#else
#define MAYBE_AsyncTerminate_WhileTaskIsRunning \
  AsyncTerminate_WhileTaskIsRunning
#endif
TEST_F(WorkerThreadTest, MAYBE_AsyncTerminate_WhileTaskIsRunning) {
  constexpr base::TimeDelta kDelay = base::Milliseconds(10);
  SetForcibleTerminationDelay(kDelay);

  ExpectReportingCallsForWorkerForciblyTerminated();
  StartWithSourceCodeNotToFinish();
  reporting_proxy_->WaitUntilScriptEvaluation();

  // Terminate() schedules a forcible termination task.
  worker_thread_->Terminate();
  EXPECT_TRUE(IsForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::kNotTerminated, GetExitCode());

  // Multiple Terminate() calls should not take effect.
  worker_thread_->Terminate();
  worker_thread_->Terminate();
  EXPECT_EQ(ExitCode::kNotTerminated, GetExitCode());

  // Wait until the forcible termination task runs.
  test::RunDelayedTasks(kDelay);
  worker_thread_->WaitForShutdownForTesting();
  EXPECT_EQ(ExitCode::kAsyncForciblyTerminated, GetExitCode());
}

TEST_F(WorkerThreadTest, SyncTerminate_WhileTaskIsRunning) {
  ExpectReportingCallsForWorkerForciblyTerminated();
  StartWithSourceCodeNotToFinish();
  reporting_proxy_->WaitUntilScriptEvaluation();

  // TerminateForTesting() synchronously terminates the worker script execution.
  worker_thread_->TerminateForTesting();
  worker_thread_->WaitForShutdownForTesting();
  EXPECT_EQ(ExitCode::kSyncForciblyTerminated, GetExitCode());
}

TEST_F(WorkerThreadTest,
       AsyncTerminateAndThenSyncTerminate_WhileTaskIsRunning) {
  SetForcibleTerminationDelay(base::Milliseconds(10));

  ExpectReportingCallsForWorkerForciblyTerminated();
  StartWithSourceCodeNotToFinish();
  reporting_proxy_->WaitUntilScriptEvaluation();

  // Terminate() schedules a forcible termination task.
  worker_thread_->Terminate();
  EXPECT_TRUE(IsForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::kNotTerminated, GetExitCode());

  // TerminateForTesting() should overtake the scheduled forcible termination
  // task.
  worker_thread_->TerminateForTesting();
  worker_thread_->WaitForShutdownForTesting();
  EXPECT_FALSE(IsForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::kSyncForciblyTerminated, GetExitCode());
}

TEST_F(WorkerThreadTest, Terminate_WhileDebuggerTaskIsRunningOnInitialization) {
  constexpr base::TimeDelta kDelay = base::Milliseconds(10);
  base::RunLoop loop;
  SetForcibleTerminationDelay(kDelay);

  EXPECT_CALL(*reporting_proxy_, DidCreateWorkerGlobalScope(_)).Times(1);
  EXPECT_CALL(*reporting_proxy_, WillDestroyWorkerGlobalScope()).Times(1);
  EXPECT_CALL(*reporting_proxy_, DidTerminateWorkerThread()).Times(1);

  auto global_scope_creation_params =
      std::make_unique<GlobalScopeCreationParams>(
          KURL("http://fake.url/"), mojom::blink::ScriptType::kClassic,
          "fake global scope name", "fake user agent", UserAgentMetadata(),
          nullptr /* web_worker_fetch_context */,
          Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
          Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
          network::mojom::ReferrerPolicy::kDefault, security_origin_.get(),
          false /* starter_secure_context */,
          CalculateHttpsState(security_origin_.get()),
          MakeGarbageCollected<WorkerClients>(),
          nullptr /* content_settings_client */,
          nullptr /* inherited_trial_features */,
          base::UnguessableToken::Create(),
          std::make_unique<WorkerSettings>(std::make_unique<Settings>().get()),
          mojom::blink::V8CacheOptions::kDefault,
          nullptr /* worklet_module_responses_map */);

  // Set wait_for_debugger so that the worker thread can pause
  // on initialization to run debugger tasks.
  auto devtools_params = std::make_unique<WorkerDevToolsParams>();
  devtools_params->wait_for_debugger = true;

  worker_thread_->Start(std::move(global_scope_creation_params),
                        WorkerBackingThreadStartupData::CreateDefault(),
                        std::move(devtools_params));

  // Used to wait for worker thread termination in a debugger task on the
  // worker thread.
  base::WaitableEvent waitable_event;
  PostCrossThreadTask(
      *worker_thread_->GetTaskRunner(TaskType::kInternalInspector), FROM_HERE,
      CrossThreadBindOnce(&WaitForSignalTask,
                          CrossThreadUnretained(worker_thread_.get()),
                          CrossThreadUnretained(&waitable_event),
                          CrossThreadOnceClosure(loop.QuitClosure())));

  // Wait for the debugger task.
  loop.Run();
  {
    base::AutoLock lock(worker_thread_->lock_);
    EXPECT_EQ(1, worker_thread_->debugger_task_counter_);
  }

  // Terminate() schedules a forcible termination task.
  worker_thread_->Terminate();
  EXPECT_TRUE(IsForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::kNotTerminated, GetExitCode());

  // Wait until the task runs. It shouldn't terminate the script execution
  // because of the running debugger task but it should get reposted.
  test::RunDelayedTasks(kDelay);
  {
    base::AutoLock lock(worker_thread_->lock_);
    EXPECT_EQ(WorkerThread::TerminationState::kPostponeTerminate,
              worker_thread_->ShouldTerminateScriptExecution());
  }
  EXPECT_TRUE(IsForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::kNotTerminated, GetExitCode());

  // Resume the debugger task. Shutdown starts after that.
  waitable_event.Signal();
  worker_thread_->WaitForShutdownForTesting();
  EXPECT_EQ(ExitCode::kGracefullyTerminated, GetExitCode());
}

TEST_F(WorkerThreadTest, Terminate_WhileDebuggerTaskIsRunning) {
  constexpr base::TimeDelta kDelay = base::Milliseconds(10);
  base::RunLoop loop;
  SetForcibleTerminationDelay(kDelay);

  ExpectReportingCalls();
  Start();
  worker_thread_->WaitForInit();

  // Used to wait for worker thread termination in a debugger task on the
  // worker thread.
  base::WaitableEvent waitable_event;
  PostCrossThreadTask(
      *worker_thread_->GetTaskRunner(TaskType::kInternalInspector), FROM_HERE,
      CrossThreadBindOnce(&WaitForSignalTask,
                          CrossThreadUnretained(worker_thread_.get()),
                          CrossThreadUnretained(&waitable_event),
                          CrossThreadOnceClosure(loop.QuitClosure())));

  // Wait for the debugger task.
  loop.Run();
  {
    base::AutoLock lock(worker_thread_->lock_);
    EXPECT_EQ(1, worker_thread_->debugger_task_counter_);
  }

  // Terminate() schedules a forcible termination task.
  worker_thread_->Terminate();
  EXPECT_TRUE(IsForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::kNotTerminated, GetExitCode());

  // Wait until the task runs. It shouldn't terminate the script execution
  // because of the running debugger task but it should get reposted.
  test::RunDelayedTasks(kDelay);
  {
    base::AutoLock lock(worker_thread_->lock_);
    EXPECT_EQ(WorkerThread::TerminationState::kPostponeTerminate,
              worker_thread_->ShouldTerminateScriptExecution());
  }
  EXPECT_TRUE(IsForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::kNotTerminated, GetExitCode());

  // Resume the debugger task. Shutdown starts after that.
  waitable_event.Signal();
  worker_thread_->WaitForShutdownForTesting();
  EXPECT_EQ(ExitCode::kGracefullyTerminated, GetExitCode());
}

// TODO(https://crbug.com/1072997): This test occasionally crashes.
TEST_F(WorkerThreadTest, DISABLED_TerminateWorkerWhileChildIsLoading) {
  base::RunLoop loop;
  ExpectReportingCalls();
  Start();
  worker_thread_->WaitForInit();

  NestedWorkerHelper nested_worker_helper;
  // Create a nested worker from the worker thread.
  PostCrossThreadTask(
      *worker_thread_->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&CreateNestedWorkerThenTerminateParent,
                          CrossThreadUnretained(worker_thread_.get()),
                          CrossThreadUnretained(&nested_worker_helper),
                          CrossThreadBindOnce(loop.QuitClosure())));
  loop.Run();

  base::WaitableEvent waitable_event;
  PostCrossThreadTask(
      *worker_thread_->GetWorkerBackingThread().BackingThread().GetTaskRunner(),
      FROM_HERE,
      CrossThreadBindOnce(&VerifyParentAndChildAreTerminated,
                          CrossThreadUnretained(worker_thread_.get()),
                          CrossThreadUnretained(&nested_worker_helper),
                          CrossThreadUnretained(&waitable_event)));
  waitable_event.Wait();
}

// Tests terminating a worker when debugger is paused.
// TODO(crbug.com/1503316): The test is flaky on Linux TSan
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_TerminateWhileWorkerPausedByDebugger \
  DISABLED_TerminateWhileWorkerPausedByDebugger
#else
#define MAYBE_TerminateWhileWorkerPausedByDebugger \
  TerminateWhileWorkerPausedByDebugger
#endif
TEST_F(WorkerThreadTest, MAYBE_TerminateWhileWorkerPausedByDebugger) {
  constexpr base::TimeDelta kDelay = base::Milliseconds(10);
  SetForcibleTerminationDelay(kDelay);

  ExpectReportingCallsForWorkerForciblyTerminated();
  StartWithSourceCodeNotToFinish();
  reporting_proxy_->WaitUntilScriptEvaluation();

  worker_thread_->GetIsolate()->RequestInterrupt(&PauseExecution,
                                                 worker_thread_.get());

  // Terminate() schedules a forcible termination task.
  worker_thread_->Terminate();
  EXPECT_TRUE(IsForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::kNotTerminated, GetExitCode());

  test::RunDelayedTasks(kDelay);
  worker_thread_->WaitForShutdownForTesting();
  EXPECT_EQ(ExitCode::kAsyncForciblyTerminated, GetExitCode());
}

// TODO(crbug.com/1503287): The test is flaky on Linux TSan
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_TerminateFrozenScript DISABLED_TerminateFrozenScript
#else
#define MAYBE_TerminateFrozenScript TerminateFrozenScript
#endif
TEST_F(WorkerThreadTest, MAYBE_TerminateFrozenScript) {
  constexpr base::TimeDelta kDelay = base::Milliseconds(10);
  SetForcibleTerminationDelay(kDelay);

  ExpectReportingCallsForWorkerForciblyTerminated();
  StartWithSourceCodeNotToFinish();
  reporting_proxy_->WaitUntilScriptEvaluation();

  base::WaitableEvent child_waitable;
  PostCrossThreadTask(
      *worker_thread_->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&base::WaitableEvent::Signal,
                          CrossThreadUnretained(&child_waitable)));

  // Freeze() enters a nested event loop where the kInternalTest should run.
  worker_thread_->Freeze(false /* is_in_back_forward_cache */);
  child_waitable.Wait();

  // Terminate() schedules a forcible termination task.
  worker_thread_->Terminate();
  EXPECT_TRUE(IsForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::kNotTerminated, GetExitCode());

  test::RunDelayedTasks(kDelay);
  worker_thread_->WaitForShutdownForTesting();
  EXPECT_EQ(ExitCode::kAsyncForciblyTerminated, GetExitCode());
}

// TODO(crbug.com/1508694): The test is flaky on Linux TSan
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_NestedPauseFreeze DISABLED_NestedPauseFreeze
#else
#define MAYBE_NestedPauseFreeze NestedPauseFreeze
#endif
TEST_F(WorkerThreadTest, MAYBE_NestedPauseFreeze) {
  constexpr base::TimeDelta kDelay = base::Milliseconds(10);
  SetForcibleTerminationDelay(kDelay);

  ExpectReportingCallsForWorkerForciblyTerminated();
  StartWithSourceCodeNotToFinish();
  reporting_proxy_->WaitUntilScriptEvaluation();

  base::WaitableEvent child_waitable;
  PostCrossThreadTask(
      *worker_thread_->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&base::WaitableEvent::Signal,
                          CrossThreadUnretained(&child_waitable)));

  // Pause() enters a nested event loop where the kInternalTest should run.
  worker_thread_->Pause();
  worker_thread_->Freeze(false /* is_in_back_forward_cache */);
  child_waitable.Wait();

  // Resume Freeze.
  worker_thread_->Resume();

  // Resume Pause.
  worker_thread_->Resume();

  // Ensure an extra Resume does nothing. Since this is called from
  // the javascript debugger API.
  worker_thread_->Resume();

  // Terminate() schedules a forcible termination task.
  worker_thread_->Terminate();
  EXPECT_TRUE(IsForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::kNotTerminated, GetExitCode());

  test::RunDelayedTasks(kDelay);
  worker_thread_->WaitForShutdownForTesting();
  EXPECT_EQ(ExitCode::kAsyncForciblyTerminated, GetExitCode());
}

// TODO(crbug.com/1508694): The test is flaky on Linux TSan
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_NestedPauseFreezeNoInterrupts \
  DISABLED_NestedPauseFreezeNoInterrupts
#else
#define MAYBE_NestedPauseFreezeNoInterrupts NestedPauseFreezeNoInterrupts
#endif
TEST_F(WorkerThreadTest, MAYBE_NestedPauseFreezeNoInterrupts) {
  constexpr base::TimeDelta kDelay = base::Milliseconds(10);
  SetForcibleTerminationDelay(kDelay);

  ExpectReportingCalls();
  Start();

  base::WaitableEvent child_waitable;
  PostCrossThreadTask(
      *worker_thread_->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&base::WaitableEvent::Signal,
                          CrossThreadUnretained(&child_waitable)));

  child_waitable.Wait();
  base::WaitableEvent child_waitable2;
  PostCrossThreadTask(
      *worker_thread_->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&base::WaitableEvent::Signal,
                          CrossThreadUnretained(&child_waitable2)));

  // Pause() enters a nested event loop where the kInternalTest should run.
  worker_thread_->Pause();
  worker_thread_->Freeze(false /* is_in_back_forward_cache */);
  child_waitable2.Wait();

  // Resume for Freeze.
  worker_thread_->Resume();

  // Resume for Pause.
  worker_thread_->Resume();

  // Ensure an extra Resume does nothing. Since this is called from
  // the javascript debugger API.
  worker_thread_->Resume();

  // Terminate() schedules a forcible termination task.
  worker_thread_->Terminate();
  EXPECT_TRUE(IsForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::kNotTerminated, GetExitCode());

  test::RunDelayedTasks(kDelay);
  worker_thread_->WaitForShutdownForTesting();
  EXPECT_EQ(ExitCode::kGracefullyTerminated, GetExitCode());
}

}  // namespace blink
