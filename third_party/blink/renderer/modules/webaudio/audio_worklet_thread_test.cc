// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <memory>
#include <tuple>

#include "base/feature_list.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/v8_cache_options.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/module_record.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/script/js_module_script.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/testing/module_test_base.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_messaging_proxy.h"
#include "third_party/blink/renderer/modules/webaudio/cross_thread_audio_worklet_processor_info.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_worklet_thread.h"
#include "third_party/blink/renderer/modules/webaudio/realtime_audio_worklet_thread.h"
#include "third_party/blink/renderer/modules/webaudio/semi_realtime_audio_worklet_thread.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

class AudioWorkletThreadTest : public PageTestBase, public ModuleTestBase {
 public:
  void SetUp() override {
    ModuleTestBase::SetUp();
    PageTestBase::SetUp(gfx::Size());
    NavigateTo(KURL("https://example.com/"));
    reporting_proxy_ = std::make_unique<WorkerReportingProxy>();
  }

  void TearDown() override {
    OfflineAudioWorkletThread::ClearSharedBackingThread();
    WorkletThreadHolder<RealtimeAudioWorkletThread>::ClearInstance();
    SemiRealtimeAudioWorkletThread::ClearSharedBackingThread();
    ModuleTestBase::TearDown();
  }

  std::unique_ptr<WorkerThread> CreateAudioWorkletThread(
      bool has_realtime_constraint,
      bool is_top_level_frame,
      base::TimeDelta realtime_buffer_duration = base::Milliseconds(3)) {
    std::unique_ptr<WorkerThread> thread =
        AudioWorkletMessagingProxy::CreateWorkletThreadWithConstraints(
            *reporting_proxy_,
            has_realtime_constraint
                ? std::optional<base::TimeDelta>(realtime_buffer_duration)
                : std::nullopt,
            is_top_level_frame);
    StartBackingThreadAndWaitUntilInit(thread.get());
    return thread;
  }

  // Attempts to run some simple script for `thread`.
  void CheckWorkletCanExecuteScript(WorkerThread* thread) {
    base::WaitableEvent wait_event;
    PostCrossThreadTask(
        *thread->GetWorkerBackingThread().BackingThread().GetTaskRunner(),
        FROM_HERE,
        CrossThreadBindOnce(&AudioWorkletThreadTest::ExecuteScriptInWorklet,
                            CrossThreadUnretained(this),
                            CrossThreadUnretained(thread),
                            CrossThreadUnretained(&wait_event)));
    wait_event.Wait();
  }

 private:
  void StartBackingThreadAndWaitUntilInit(WorkerThread* thread) {
    LocalDOMWindow* window = GetFrame().DomWindow();
    thread->Start(
        std::make_unique<GlobalScopeCreationParams>(
            window->Url(), mojom::blink::ScriptType::kModule, "AudioWorklet",
            window->UserAgent(),
            window->GetFrame()->Loader().UserAgentMetadata(),
            nullptr /* web_worker_fetch_context */,
            Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
            Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
            window->GetReferrerPolicy(), window->GetSecurityOrigin(),
            window->IsSecureContext(), window->GetHttpsState(),
            nullptr /* worker_clients */, nullptr /* content_settings_client */,
            OriginTrialContext::GetInheritedTrialFeatures(window).get(),
            base::UnguessableToken::Create(), nullptr /* worker_settings */,
            mojom::blink::V8CacheOptions::kDefault,
            MakeGarbageCollected<WorkletModuleResponsesMap>(),
            mojo::NullRemote() /* browser_interface_broker */,
            window->GetFrame()->Loader().CreateWorkerCodeCacheHost(),
            window->GetFrame()->GetBlobUrlStorePendingRemote(),
            BeginFrameProviderParams(), nullptr /* parent_permissions_policy */,
            window->GetAgentClusterID(), ukm::kInvalidSourceId,
            window->GetExecutionContextToken()),
        std::optional(WorkerBackingThreadStartupData::CreateDefault()),
        std::make_unique<WorkerDevToolsParams>());

    // Wait until the cross-thread initialization is completed.
    base::WaitableEvent completion_event;
    PostCrossThreadTask(
        *thread->GetWorkerBackingThread().BackingThread().GetTaskRunner(),
        FROM_HERE,
        CrossThreadBindOnce(&base::WaitableEvent::Signal,
                            CrossThreadUnretained(&completion_event)));
    completion_event.Wait();
  }

  void ExecuteScriptInWorklet(WorkerThread* thread,
                              base::WaitableEvent* wait_event) {
    ScriptState* script_state =
        thread->GlobalScope()->ScriptController()->GetScriptState();
    EXPECT_TRUE(script_state);
    ScriptState::Scope scope(script_state);
    KURL js_url("https://example.com/worklet.js");
    v8::Local<v8::Module> module = ModuleTestBase::CompileModule(
        script_state, "var counter = 0; ++counter;", js_url);
    EXPECT_FALSE(module.IsEmpty());
    ScriptValue exception =
        ModuleRecord::Instantiate(script_state, module, js_url);
    EXPECT_TRUE(exception.IsEmpty());
    ScriptEvaluationResult result =
        JSModuleScript::CreateForTest(Modulator::From(script_state), module,
                                      js_url)
            ->RunScriptOnScriptStateAndReturnValue(script_state);
    EXPECT_EQ(result.GetResultType(),
              ScriptEvaluationResult::ResultType::kSuccess);
    wait_event->Signal();
  }

  std::unique_ptr<WorkerReportingProxy> reporting_proxy_;
};

TEST_F(AudioWorkletThreadTest, Basic) {
  std::unique_ptr<WorkerThread> audio_worklet_thread =
      CreateAudioWorkletThread(true, true);
  CheckWorkletCanExecuteScript(audio_worklet_thread.get());
  audio_worklet_thread->Terminate();
  audio_worklet_thread->WaitForShutdownForTesting();
}

// Creates 2 different AudioWorkletThreads with different RT constraints.
// Checks if they are running on a different thread.
TEST_F(AudioWorkletThreadTest, CreateDifferentWorkletThreadsAndTerminate_1) {
  // Create RealtimeAudioWorkletThread.
  std::unique_ptr<WorkerThread> first_worklet_thread =
      CreateAudioWorkletThread(true, true);
  Thread* first_backing_thread =
      &first_worklet_thread->GetWorkerBackingThread().BackingThread();
  v8::Isolate* first_isolate = first_worklet_thread->GetIsolate();

  // Create OfflineAudioWorkletThread.
  std::unique_ptr<WorkerThread> second_worklet_thread =
      CreateAudioWorkletThread(false, true);
  Thread* second_backing_thread =
      &second_worklet_thread->GetWorkerBackingThread().BackingThread();
  v8::Isolate* second_isolate = second_worklet_thread->GetIsolate();

  // Check if they are two different threads, and two different v8::isolates.
  ASSERT_NE(first_backing_thread, second_backing_thread);
  ASSERT_NE(first_isolate, second_isolate);

  first_worklet_thread->Terminate();
  first_worklet_thread->WaitForShutdownForTesting();
  second_worklet_thread->Terminate();
  second_worklet_thread->WaitForShutdownForTesting();
}

// Creates 2 AudioWorkletThreads with RT constraint from 2 different
// originating frames. Checks if they are running on a different thread.
TEST_F(AudioWorkletThreadTest, CreateDifferentWorkletThreadsAndTerminate_2) {
  // Create an AudioWorkletThread from a main frame with RT constraint.
  std::unique_ptr<WorkerThread> first_worklet_thread =
      CreateAudioWorkletThread(true, true);
  Thread* first_backing_thread =
      &first_worklet_thread->GetWorkerBackingThread().BackingThread();
  v8::Isolate* first_isolate = first_worklet_thread->GetIsolate();

  // Create an AudioWorkletThread from a sub frame with RT constraint.
  std::unique_ptr<WorkerThread> second_worklet_thread =
      CreateAudioWorkletThread(true, false);
  Thread* second_backing_thread =
      &second_worklet_thread->GetWorkerBackingThread().BackingThread();
  v8::Isolate* second_isolate = second_worklet_thread->GetIsolate();

  // Check if they are two different threads, and two different v8::isolates.
  ASSERT_NE(first_backing_thread, second_backing_thread);
  ASSERT_NE(first_isolate, second_isolate);

  first_worklet_thread->Terminate();
  first_worklet_thread->WaitForShutdownForTesting();
  second_worklet_thread->Terminate();
  second_worklet_thread->WaitForShutdownForTesting();
}

class AudioWorkletThreadInteractionTest
    : public AudioWorkletThreadTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  AudioWorkletThreadInteractionTest()
      : has_realtime_constraint_(std::get<0>(GetParam())),
        is_top_level_frame_(std::get<1>(GetParam())) {}

 protected:
  const bool has_realtime_constraint_;
  const bool is_top_level_frame_;
};

TEST_P(AudioWorkletThreadInteractionTest, CreateSecondAndTerminateFirst) {
  // Create the first worklet and wait until it is initialized.
  std::unique_ptr<WorkerThread> first_worklet_thread =
      CreateAudioWorkletThread(has_realtime_constraint_, is_top_level_frame_);
  Thread* first_backing_thread =
      &first_worklet_thread->GetWorkerBackingThread().BackingThread();
  CheckWorkletCanExecuteScript(first_worklet_thread.get());
  v8::Isolate* first_isolate = first_worklet_thread->GetIsolate();
  ASSERT_TRUE(first_isolate);

  // Create the second worklet and immediately destroy the first worklet.
  std::unique_ptr<WorkerThread> second_worklet_thread =
      CreateAudioWorkletThread(has_realtime_constraint_, is_top_level_frame_);
  Thread* second_backing_thread =
      &second_worklet_thread->GetWorkerBackingThread().BackingThread();
  CheckWorkletCanExecuteScript(second_worklet_thread.get());
  v8::Isolate* second_isolate = second_worklet_thread->GetIsolate();
  ASSERT_TRUE(second_isolate);

  // We don't use terminateAndWait here to avoid forcible termination.
  first_worklet_thread->Terminate();
  first_worklet_thread->WaitForShutdownForTesting();

  // Wait until the second worklet is initialized. Verify the equality of the
  // thread and the isolate of two instances; if it's for a real-time
  // BaseAudioContext and it's from a top-level frame, it should use different,
  // dedicated backing threads.
  if (has_realtime_constraint_ && is_top_level_frame_) {
    ASSERT_NE(first_backing_thread, second_backing_thread);
    ASSERT_NE(first_isolate, second_isolate);
  } else {
    ASSERT_EQ(first_backing_thread, second_backing_thread);
    ASSERT_EQ(first_isolate, second_isolate);
  }

  second_worklet_thread->Terminate();
  second_worklet_thread->WaitForShutdownForTesting();
}

TEST_P(AudioWorkletThreadInteractionTest, TerminateFirstAndCreateSecond) {
  // Create the first worklet, wait until it is initialized, and terminate it.
  std::unique_ptr<WorkerThread> worklet_thread =
      CreateAudioWorkletThread(has_realtime_constraint_, is_top_level_frame_);
  Thread* first_backing_thread =
      &worklet_thread->GetWorkerBackingThread().BackingThread();
  CheckWorkletCanExecuteScript(worklet_thread.get());

  // We don't use terminateAndWait here to avoid forcible termination.
  worklet_thread->Terminate();
  worklet_thread->WaitForShutdownForTesting();

  // Create the second worklet. The backing thread is same.
  worklet_thread =
      CreateAudioWorkletThread(has_realtime_constraint_, is_top_level_frame_);
  Thread* second_backing_thread =
      &worklet_thread->GetWorkerBackingThread().BackingThread();
  CheckWorkletCanExecuteScript(worklet_thread.get());

  if (has_realtime_constraint_ && is_top_level_frame_) {
    ASSERT_NE(first_backing_thread, second_backing_thread);
  } else {
    ASSERT_EQ(first_backing_thread, second_backing_thread);
  }

  worklet_thread->Terminate();
  worklet_thread->WaitForShutdownForTesting();
}

TEST_P(AudioWorkletThreadInteractionTest,
       ThreadManagementSystemForRealtimeAndTopLevelFrame) {
  // Creates 5 AudioWorkletThreads; based on the configuration (RT constraint,
  // frame level) they could be either RealtimeAudioWorkletThread,
  // SemiRealtimeAudioWorkletThread, or OfflineAudioWorkletThread with
  // different backing threads.
  constexpr int number_of_threads = 5;
  std::unique_ptr<WorkerThread> worklet_threads[number_of_threads];
  Thread* worklet_backing_threads[number_of_threads];
  for (int i = 0; i < number_of_threads; i++) {
    worklet_threads[i] =
        CreateAudioWorkletThread(has_realtime_constraint_, is_top_level_frame_);
    worklet_backing_threads[i] =
        &worklet_threads[i]->GetWorkerBackingThread().BackingThread();
  }

  if (has_realtime_constraint_ && is_top_level_frame_) {
    // For realtime contexts on a top-level frame, the first 3 worklet backing
    // threads are unique and do not share a backing thread.
    ASSERT_NE(worklet_backing_threads[0], worklet_backing_threads[1]);
    ASSERT_NE(worklet_backing_threads[0], worklet_backing_threads[2]);
    ASSERT_NE(worklet_backing_threads[1], worklet_backing_threads[2]);
    // They also differ from the 4th worklet backing thread, which is shared by
    // all subsequent AudioWorklet instances.
    ASSERT_NE(worklet_backing_threads[0], worklet_backing_threads[3]);
    ASSERT_NE(worklet_backing_threads[1], worklet_backing_threads[3]);
    ASSERT_NE(worklet_backing_threads[2], worklet_backing_threads[3]);
  } else {
    // For all other cases, a single worklet backing thread is shared by
    // multiple AudioWorklets.
    ASSERT_EQ(worklet_backing_threads[0], worklet_backing_threads[1]);
    ASSERT_EQ(worklet_backing_threads[0], worklet_backing_threads[2]);
    ASSERT_EQ(worklet_backing_threads[0], worklet_backing_threads[3]);
  }

  // In any case, all AudioWorklets after 4th instance will shared a single
  // backing thread.
  ASSERT_EQ(worklet_backing_threads[3], worklet_backing_threads[4]);

  if (has_realtime_constraint_ && is_top_level_frame_) {
    // Shut down the 3rd thread and verify 2 other dedicated threads are still
    // running.
    worklet_backing_threads[2] = nullptr;
    worklet_threads[2]->Terminate();
    worklet_threads[2]->WaitForShutdownForTesting();
    worklet_threads[2].reset();

    ASSERT_EQ(worklet_threads[0]->GetExitCodeForTesting(),
              WorkerThread::ExitCode::kNotTerminated);
    ASSERT_EQ(worklet_threads[1]->GetExitCodeForTesting(),
              WorkerThread::ExitCode::kNotTerminated);

    // Create a new thread and verify if 3 dedicated threads are running.
    std::unique_ptr<WorkerThread> new_worklet_thread =
        CreateAudioWorkletThread(has_realtime_constraint_, is_top_level_frame_);
    Thread* new_worklet_backing_thread =
          &new_worklet_thread->GetWorkerBackingThread().BackingThread();

    ASSERT_NE(worklet_backing_threads[0], new_worklet_backing_thread);
    ASSERT_NE(worklet_backing_threads[1], new_worklet_backing_thread);

    // It also should be different from a shared backing thread.
    ASSERT_NE(worklet_backing_threads[3], new_worklet_backing_thread);

    new_worklet_thread->Terminate();
    new_worklet_thread->WaitForShutdownForTesting();
  }

  // Shutting down one of worklet threads on a shared backing thread should not
  // affect other worklet threads.
  worklet_backing_threads[4] = nullptr;
  worklet_threads[4]->Terminate();
  worklet_threads[4]->WaitForShutdownForTesting();
  worklet_threads[4].reset();

  ASSERT_EQ(worklet_threads[3]->GetExitCodeForTesting(),
            WorkerThread::ExitCode::kNotTerminated);

  // Cleaning up remaining worklet threads.
  for (auto& worklet_thread : worklet_threads) {
    if (worklet_thread.get()) {
      worklet_thread->Terminate();
      worklet_thread->WaitForShutdownForTesting();
    }
  }
}

INSTANTIATE_TEST_SUITE_P(AudioWorkletThreadInteractionTestGroup,
                         AudioWorkletThreadInteractionTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

struct ThreadPriorityTestParam {
  const bool has_realtime_constraint;
  const bool is_top_level_frame;
  const bool is_enabled_by_finch;
  const base::ThreadPriorityForTest expected_priority;
};

constexpr ThreadPriorityTestParam kThreadPriorityTestParams[] = {
    // RT thread enabled by Finch.
    {true, true, true, base::ThreadPriorityForTest::kRealtimeAudio},

    // RT thread disabled by Finch.
    {true, true, false, base::ThreadPriorityForTest::kNormal},

    // Non-main frame, RT thread enabled by Finch.
    {true, false, true, base::ThreadPriorityForTest::kDisplay},

    // Non-main frame, RT thread disabled by Finch.
    {true, false, false, base::ThreadPriorityForTest::kNormal},

    // The OfflineAudioContext always uses a NORMAL priority thread.
    {false, true, true, base::ThreadPriorityForTest::kNormal},
    {false, true, false, base::ThreadPriorityForTest::kNormal},
    {false, false, true, base::ThreadPriorityForTest::kNormal},
    {false, false, false, base::ThreadPriorityForTest::kNormal},
};

class AudioWorkletThreadPriorityTest
    : public AudioWorkletThreadTest,
      public testing::WithParamInterface<ThreadPriorityTestParam> {
 public:
  void InitWithRealtimePrioritySettings(bool is_enabled_by_finch) {
    std::vector<base::test::FeatureRef> enabled;
    std::vector<base::test::FeatureRef> disabled;
    if (is_enabled_by_finch) {
      enabled.push_back(features::kAudioWorkletThreadRealtimePriority);
    } else {
      disabled.push_back(features::kAudioWorkletThreadRealtimePriority);
    }
    feature_list_.InitWithFeatures(enabled, disabled);
  }

  void CreateCheckThreadPriority(
      bool has_realtime_constraint,
      bool is_top_level_frame,
      base::ThreadPriorityForTest expected_priority) {
    std::unique_ptr<WorkerThread> audio_worklet_thread =
        CreateAudioWorkletThread(has_realtime_constraint, is_top_level_frame);
    WorkerThread* thread = audio_worklet_thread.get();
    base::WaitableEvent wait_event;
    PostCrossThreadTask(
        *thread->GetWorkerBackingThread().BackingThread().GetTaskRunner(),
        FROM_HERE,
        CrossThreadBindOnce(
            &AudioWorkletThreadPriorityTest::CheckThreadPriorityOnWorkerThread,
            CrossThreadUnretained(this),
            CrossThreadUnretained(thread),
            expected_priority,
            CrossThreadUnretained(&wait_event)));
    wait_event.Wait();
    audio_worklet_thread->Terminate();
    audio_worklet_thread->WaitForShutdownForTesting();
  }

 private:
  void CheckThreadPriorityOnWorkerThread(
      WorkerThread* thread,
      base::ThreadPriorityForTest expected_priority,
      base::WaitableEvent* wait_event) {
    ASSERT_TRUE(thread->IsCurrentThread());
    base::ThreadPriorityForTest actual_priority =
        base::PlatformThread::GetCurrentThreadPriorityForTest();

    // TODO(crbug.com/1022888): The worklet thread priority is always NORMAL
    // on OS_LINUX and OS_CHROMEOS regardless of the thread priority setting.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    if (expected_priority == base::ThreadPriorityForTest::kRealtimeAudio ||
        expected_priority == base::ThreadPriorityForTest::kDisplay) {
      EXPECT_EQ(actual_priority, base::ThreadPriorityForTest::kNormal);
    } else {
      EXPECT_EQ(actual_priority, expected_priority);
    }
#else
    EXPECT_EQ(actual_priority, expected_priority);
#endif

    wait_event->Signal();
  }
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(AudioWorkletThreadPriorityTest, CheckThreadPriority) {
  const auto& test_param = GetParam();
  InitWithRealtimePrioritySettings(test_param.is_enabled_by_finch);
  CreateCheckThreadPriority(test_param.has_realtime_constraint,
                            test_param.is_top_level_frame,
                            test_param.expected_priority);
}

INSTANTIATE_TEST_SUITE_P(AudioWorkletThreadPriorityTestGroup,
                         AudioWorkletThreadPriorityTest,
                         testing::ValuesIn(kThreadPriorityTestParams));

}  // namespace blink

#if BUILDFLAG(IS_APPLE)

namespace WTF {
template <>
struct CrossThreadCopier<base::TimeDelta>
    : public CrossThreadCopierPassThrough<base::TimeDelta> {
  STATIC_ONLY(CrossThreadCopier);
};
}  // namespace WTF

namespace blink {

class AudioWorkletRealtimePeriodTestMac : public AudioWorkletThreadTest {
 public:
  std::unique_ptr<WorkerThread> CreateThreadAndCheckRealtimePeriod(
      base::TimeDelta realtime_buffer_duration,
      base::TimeDelta expected_realtime_period) {
    std::unique_ptr<WorkerThread> audio_worklet_thread =
        CreateAudioWorkletThread(/*has_realtime_constraint=*/true,
                                 /*is_top_level_frame=*/true,
                                 realtime_buffer_duration);
    WorkerThread* thread = audio_worklet_thread.get();
    base::WaitableEvent wait_event;
    PostCrossThreadTask(
        *thread->GetWorkerBackingThread().BackingThread().GetTaskRunner(),
        FROM_HERE,
        CrossThreadBindOnce(
            &AudioWorkletRealtimePeriodTestMac::
                CheckThreadRealtimePeriodOnWorkerThread,
            CrossThreadUnretained(this), CrossThreadUnretained(thread),
            expected_realtime_period, CrossThreadUnretained(&wait_event)));
    wait_event.Wait();
    return audio_worklet_thread;
  }

 private:
  void CheckThreadRealtimePeriodOnWorkerThread(
      WorkerThread* thread,
      base::TimeDelta expected_realtime_period,
      base::WaitableEvent* wait_event) {
    ASSERT_TRUE(thread->IsCurrentThread());

    base::ThreadPriorityForTest actual_priority =
        base::PlatformThread::GetCurrentThreadPriorityForTest();

    base::TimeDelta actual_realtime_period =
        base::PlatformThread::GetCurrentThreadRealtimePeriodForTest();

    EXPECT_EQ(actual_priority, base::ThreadPriorityForTest::kRealtimeAudio);
    EXPECT_EQ(actual_realtime_period, expected_realtime_period);

    wait_event->Signal();
  }
};

TEST_F(AudioWorkletRealtimePeriodTestMac, CheckRealtimePeriod) {
  // Creates 5 realtime AudioWorkletThreads with different realtime buffer
  // durations; the last two will be sharing the same backing thread.
  base::TimeDelta realtime_buffer_durations[] = {
      base::Milliseconds(10), base::Milliseconds(20), base::Milliseconds(30),
      base::Milliseconds(40), base::Milliseconds(50)};

  std::vector<std::unique_ptr<WorkerThread>> worklet_threads;
  worklet_threads.push_back(CreateThreadAndCheckRealtimePeriod(
      realtime_buffer_durations[0], realtime_buffer_durations[0]));
  worklet_threads.push_back(CreateThreadAndCheckRealtimePeriod(
      realtime_buffer_durations[1], realtime_buffer_durations[1]));
  worklet_threads.push_back(CreateThreadAndCheckRealtimePeriod(
      realtime_buffer_durations[2], realtime_buffer_durations[2]));
  worklet_threads.push_back(CreateThreadAndCheckRealtimePeriod(
      realtime_buffer_durations[3], realtime_buffer_durations[3]));
  // Note: we expect that the last two worklets share the same backng thread, so
  // the should have the same realtime period.
  worklet_threads.push_back(CreateThreadAndCheckRealtimePeriod(
      realtime_buffer_durations[4], realtime_buffer_durations[3]));

  for (auto& worklet_thread : worklet_threads) {
    if (worklet_thread.get()) {
      worklet_thread->Terminate();
      worklet_thread->WaitForShutdownForTesting();
    }
  }
}

}  // namespace blink

#endif  // BUILDFLAG(IS_APPLE)
