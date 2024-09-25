// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_thread_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/heap_array.h"
#include "base/debug/leak_annotations.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/discardable_memory.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/structured_shared_memory.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/test_switches.h"
#include "base/time/time.h"
#include "cc/base/switches.h"
#include "content/app/mojo/mojo_init.h"
#include "content/common/in_process_child_thread_params.h"
#include "content/common/pseudonymization_salt.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/child_process_host_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/test_launcher.h"
#include "content/renderer/render_process_impl.h"
#include "gpu/config/gpu_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/performance/performance_scenarios.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/scheduler/test/web_mock_thread_scheduler.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "ui/base/ui_base_switches.h"

// IPC messages for testing ----------------------------------------------------

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
// TODO(mdempsky): Fix properly by moving into a separate
// browsertest_message_generator.cc file.
#undef IPC_IPC_MESSAGE_MACROS_H_
#undef IPC_MESSAGE_EXTRA
#define IPC_MESSAGE_IMPL
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"
#include "ipc/ipc_message_templates_impl.h"

#undef IPC_MESSAGE_START
#define IPC_MESSAGE_START TestMsgStart
IPC_MESSAGE_CONTROL0(TestMsg_QuitRunLoop)

#endif

// -----------------------------------------------------------------------------

// These tests leak memory, this macro disables the test when under the
// LeakSanitizer.
#ifdef LEAK_SANITIZER
#define WILL_LEAK(NAME) DISABLED_##NAME
#else
#define WILL_LEAK(NAME) NAME
#endif

namespace content {

// FIXME: It would be great if there was a reusable mock SingleThreadTaskRunner
class TestTaskCounter : public base::SingleThreadTaskRunner {
 public:
  TestTaskCounter() : count_(0) {}

  // SingleThreadTaskRunner implementation.
  bool PostDelayedTask(const base::Location&,
                       base::OnceClosure,
                       base::TimeDelta) override {
    base::AutoLock auto_lock(lock_);
    count_++;
    return true;
  }

  bool PostNonNestableDelayedTask(const base::Location&,
                                  base::OnceClosure,
                                  base::TimeDelta) override {
    base::AutoLock auto_lock(lock_);
    count_++;
    return true;
  }

  bool RunsTasksInCurrentSequence() const override { return true; }

  int NumTasksPosted() const {
    base::AutoLock auto_lock(lock_);
    return count_;
  }

 private:
  ~TestTaskCounter() override {}

  mutable base::Lock lock_;
  int count_;
};

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
class QuitOnTestMsgFilter : public IPC::MessageFilter {
 public:
  explicit QuitOnTestMsgFilter(base::OnceClosure quit_closure)
      : origin_task_runner_(
            blink::scheduler::GetSequencedTaskRunnerForTesting()),
        quit_closure_(std::move(quit_closure)) {}

  // IPC::MessageFilter overrides:
  bool OnMessageReceived(const IPC::Message& message) override {
    origin_task_runner_->PostTask(FROM_HERE, std::move(quit_closure_));
    return true;
  }

  bool GetSupportedMessageClasses(
      std::vector<uint32_t>* supported_message_classes) const override {
    supported_message_classes->push_back(TestMsgStart);
    return true;
  }

 private:
  ~QuitOnTestMsgFilter() override {}

  scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;
  base::OnceClosure quit_closure_;
};
#endif

class RenderThreadImplBrowserTest : public testing::Test,
                                    public ChildProcessHostDelegate {
 public:
  RenderThreadImplBrowserTest() {}

  RenderThreadImplBrowserTest(const RenderThreadImplBrowserTest&) = delete;
  RenderThreadImplBrowserTest& operator=(const RenderThreadImplBrowserTest&) =
      delete;

  void SetUp() override {
    content_renderer_client_ = std::make_unique<ContentRendererClient>();
    SetRendererClientForTesting(content_renderer_client_.get());

    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
        GetIOThreadTaskRunner({});

    InitializeMojo();
    process_host_ =
        ChildProcessHost::Create(this, ChildProcessHost::IpcMode::kNormal);
    process_host_->CreateChannelMojo();

    CHECK(!process_.get());
    process_ = std::make_unique<RenderProcess>();
    test_task_counter_ = base::MakeRefCounted<TestTaskCounter>();

    // RenderThreadImpl expects the browser to pass these flags.
    base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
    base::CommandLine::StringVector old_argv = cmd->argv();

    cmd->AppendSwitchASCII(switches::kLang, "en-US");

    cmd->AppendSwitchASCII(cc::switches::kNumRasterThreads, "1");

    // To avoid creating a GPU channel to query if
    // accelerated_video_decode is blocklisted on older Android system
    // in RenderThreadImpl::Init().
    cmd->AppendSwitch(switches::kIgnoreGpuBlocklist);

    ContentTestSuiteBase::InitializeResourceBundle();

    blink::Platform::InitializeBlink();
    auto main_thread_scheduler =
        blink::scheduler::CreateMockWebMainThreadSchedulerForTests();
    scoped_refptr<base::SingleThreadTaskRunner> test_task_counter(
        test_task_counter_.get());

    // This handles the --force-fieldtrials flag passed to content_browsertests.
    base::FieldTrialList::CreateTrialsFromString(
        cmd->GetSwitchValueASCII(::switches::kForceFieldTrials));
    thread_ = new RenderThreadImpl(
        InProcessChildThreadParams(io_task_runner,
                                   &process_host_->GetMojoInvitation().value()),
        /*renderer_client_id=*/1, std::move(main_thread_scheduler));
    cmd->InitFromArgv(old_argv);

    run_loop_ = std::make_unique<base::RunLoop>();
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
    test_msg_filter_ = base::MakeRefCounted<QuitOnTestMsgFilter>(
        run_loop_->QuitWhenIdleClosure());
    thread_->AddFilter(test_msg_filter_.get());
#endif

    main_thread_scheduler_ =
        static_cast<blink::scheduler::WebMockThreadScheduler*>(
            thread_->GetWebMainThreadScheduler());
  }

  void TearDown() override {
    SetRendererClientForTesting(nullptr);
    CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kSingleProcessTests));
    // In a single-process mode, we need to avoid destructing `process_`
    // because it will call _exit(0) and kill the process before the browser
    // side is ready to exit.
    ANNOTATE_LEAKING_OBJECT_PTR(process_.get());
    // TODO(crbug.com/40771960): `StopIOThreadForTesting()` is a stop-gap
    // solution to fix flaky tests (see crbug.com/1126157). The underlying
    // reason for this issue is that the `RenderThreadImpl` created in `SetUp()`
    // above actually shares its main thread with the browser's, which is
    // inconsistent with how in-process renderers work in production and other
    // tests. Despite sharing its main thread with the browser, it still has its
    // own IO thread (owned and created by `ChildProcess`). In these tests, the
    // `BrowserTaskEnvironment` has no idea about this separate renderer IO
    // thread, which can post tasks back to the browser's main thread. During
    // `BrowserTaskEnvironment` shutdown, it CHECK()s that after the threads are
    // stopped and flushed, no other tasks exist on its SequenceManager's task
    // queues. However if we don't stop the IO thread here, then it may continue
    // to post tasks to the `BrowserTaskEnvironment`'s main thread, causing the
    // CHECK() to get hit. We should really fix the above tests to create a
    // `RenderThreadImpl` on its own thread the traditional route, but this fix
    // will work until we have the time to explore that option.
    process_->StopIOThreadForTesting();
    process_.release();
  }

  // ChildProcessHostDelegate implementation:
  bool OnMessageReceived(const IPC::Message&) override { return true; }
  const base::Process& GetProcess() override { return null_process_; }

 protected:
  IPC::Sender* sender() { return process_host_.get(); }

  void SetBackgroundState(base::Process::Priority process_priority) {
    mojom::Renderer* renderer_interface = thread_;
    const mojom::RenderProcessVisibleState visible_state =
        RendererIsHidden() ? mojom::RenderProcessVisibleState::kHidden
                           : mojom::RenderProcessVisibleState::kVisible;
    renderer_interface->SetProcessState(process_priority, visible_state);
  }

  void SetVisibleState(mojom::RenderProcessVisibleState visible_state) {
    mojom::Renderer* renderer_interface = thread_;
    const base::Process::Priority process_priority =
        RendererIsBackgrounded() ? base::Process::Priority::kBestEffort
                                 : base::Process::Priority::kUserBlocking;
    renderer_interface->SetProcessState(process_priority, visible_state);
  }

  bool RendererIsBackgrounded() { return thread_->RendererIsBackgrounded(); }
  bool RendererIsHidden() { return thread_->RendererIsHidden(); }

  scoped_refptr<TestTaskCounter> test_task_counter_;

  // Must be created before TestContentClientInitializer, since with
  // --force-renderer-accessibility that creates a BrowserAccessibilityStateImpl
  // that uses a browser thread.
  BrowserTaskEnvironment browser_threads_{
      BrowserTaskEnvironment::REAL_IO_THREAD};

  TestContentClientInitializer content_client_initializer_;
  std::unique_ptr<ContentRendererClient> content_renderer_client_;

  const base::Process null_process_;
  std::unique_ptr<ChildProcessHost> process_host_;

  std::unique_ptr<RenderProcess> process_;
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  scoped_refptr<QuitOnTestMsgFilter> test_msg_filter_;
#endif

  raw_ptr<blink::scheduler::WebMockThreadScheduler> main_thread_scheduler_;

  // RenderThreadImpl doesn't currently support a proper shutdown sequence
  // and it's okay when we're running in multi-process mode because renderers
  // get killed by the OS. Memory leaks aren't nice but it's test-only.
  raw_ptr<RenderThreadImpl> thread_;

  std::unique_ptr<base::RunLoop> run_loop_;
};

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
// Disabled under LeakSanitizer due to memory leaks.
TEST_F(RenderThreadImplBrowserTest,
       WILL_LEAK(NonResourceDispatchIPCTasksDontGoThroughScheduler)) {
  // This seems to deflake the test on Android.
  browser_threads_.RunIOThreadUntilIdle();

  // NOTE other than not being a resource message, the actual message is
  // unimportant.
  sender()->Send(new TestMsg_QuitRunLoop());

  // In-process RenderThreadImpl does not start a browser loop so the random
  // browser seed is never generated. To allow the ChildProcessHost to correctly
  // send a seed to the ChildProcess without hitting a DCHECK, set the seed to
  // an arbitrary non-zero value.
  SetPseudonymizationSalt(0xDEADBEEF);

  run_loop_->Run();

  EXPECT_EQ(0, test_task_counter_->NumTasksPosted());
}
#endif

TEST_F(RenderThreadImplBrowserTest, RendererIsBackgrounded) {
  SetBackgroundState(base::Process::Priority::kBestEffort);
  EXPECT_TRUE(RendererIsBackgrounded());

  SetBackgroundState(base::Process::Priority::kUserBlocking);
  EXPECT_FALSE(RendererIsBackgrounded());
}

TEST_F(RenderThreadImplBrowserTest, RendererIsHidden) {
  SetVisibleState(mojom::RenderProcessVisibleState::kHidden);
  EXPECT_TRUE(RendererIsHidden());

  SetVisibleState(mojom::RenderProcessVisibleState::kVisible);
  EXPECT_FALSE(RendererIsHidden());
}

TEST_F(RenderThreadImplBrowserTest, RendererStateTransitionVisible) {
  // Going from an unknown to a visible state should mark the renderer as
  // foregrounded and visible.
  EXPECT_CALL(*main_thread_scheduler_, SetRendererBackgrounded(false));
  EXPECT_CALL(*main_thread_scheduler_, SetRendererHidden(false));
  EXPECT_CALL(*main_thread_scheduler_, SetRendererBackgrounded(true)).Times(0);
  EXPECT_CALL(*main_thread_scheduler_, SetRendererHidden(true)).Times(0);
  SetVisibleState(mojom::RenderProcessVisibleState::kVisible);
  testing::Mock::VerifyAndClear(main_thread_scheduler_);

  // Going from a hidden to a visible state should mark the renderer as visible.
  SetVisibleState(mojom::RenderProcessVisibleState::kHidden);
  EXPECT_CALL(*main_thread_scheduler_, SetRendererHidden(false));
  EXPECT_CALL(*main_thread_scheduler_, SetRendererBackgrounded(false)).Times(0);
  EXPECT_CALL(*main_thread_scheduler_, SetRendererBackgrounded(true)).Times(0);
  EXPECT_CALL(*main_thread_scheduler_, SetRendererHidden(true)).Times(0);
  SetVisibleState(mojom::RenderProcessVisibleState::kVisible);
  testing::Mock::VerifyAndClear(main_thread_scheduler_);

  // Going from a visible to a hidden state should mark the renderer as hidden.
  EXPECT_CALL(*main_thread_scheduler_, SetRendererBackgrounded(false)).Times(0);
  EXPECT_CALL(*main_thread_scheduler_, SetRendererHidden(false)).Times(0);
  EXPECT_CALL(*main_thread_scheduler_, SetRendererBackgrounded(true)).Times(0);
  EXPECT_CALL(*main_thread_scheduler_, SetRendererHidden(true));
  SetVisibleState(mojom::RenderProcessVisibleState::kHidden);
  testing::Mock::VerifyAndClear(main_thread_scheduler_);

  testing::Mock::AllowLeak(main_thread_scheduler_);
}

TEST_F(RenderThreadImplBrowserTest, RendererStateTransitionHidden) {
  // Going from an unknown to a visible state should mark the renderer as
  // foregrounded and hidden.
  EXPECT_CALL(*main_thread_scheduler_, SetRendererBackgrounded(false));
  EXPECT_CALL(*main_thread_scheduler_, SetRendererHidden(true));
  EXPECT_CALL(*main_thread_scheduler_, SetRendererBackgrounded(true)).Times(0);
  EXPECT_CALL(*main_thread_scheduler_, SetRendererHidden(false)).Times(0);
  SetVisibleState(mojom::RenderProcessVisibleState::kHidden);
  testing::Mock::VerifyAndClear(main_thread_scheduler_);

  testing::Mock::AllowLeak(main_thread_scheduler_);
}

TEST_F(RenderThreadImplBrowserTest, RendererStateTransitionBackgrounded) {
  // Going from an unknown to a backgrounded state should mark the renderer as
  // backgrounded but not hidden.
  EXPECT_CALL(*main_thread_scheduler_, SetRendererHidden(true)).Times(0);
  EXPECT_CALL(*main_thread_scheduler_, SetRendererBackgrounded(true));
  EXPECT_CALL(*main_thread_scheduler_, SetRendererHidden(false));
  EXPECT_CALL(*main_thread_scheduler_, SetRendererBackgrounded(false)).Times(0);
  SetBackgroundState(base::Process::Priority::kBestEffort);
  testing::Mock::VerifyAndClear(main_thread_scheduler_);

  // Going from a backgrounded to a foregrounded state should mark the renderer
  // as foregrounded.
  EXPECT_CALL(*main_thread_scheduler_, SetRendererBackgrounded(true)).Times(0);
  EXPECT_CALL(*main_thread_scheduler_, SetRendererHidden(true)).Times(0);
  EXPECT_CALL(*main_thread_scheduler_, SetRendererHidden(false)).Times(0);
  EXPECT_CALL(*main_thread_scheduler_, SetRendererBackgrounded(false));
  SetBackgroundState(base::Process::Priority::kUserBlocking);
  testing::Mock::VerifyAndClear(main_thread_scheduler_);

  // Going from a foregrounded state to another foregrounded state should not
  // remark the renderer as foregrounded.
  EXPECT_CALL(*main_thread_scheduler_, SetRendererBackgrounded(true)).Times(0);
  EXPECT_CALL(*main_thread_scheduler_, SetRendererHidden(true)).Times(0);
  EXPECT_CALL(*main_thread_scheduler_, SetRendererHidden(false)).Times(0);
  EXPECT_CALL(*main_thread_scheduler_, SetRendererBackgrounded(false)).Times(0);
  SetBackgroundState(base::Process::Priority::kUserVisible);
  testing::Mock::VerifyAndClear(main_thread_scheduler_);

  // Going from a foregrounded to a backgrounded state should mark the renderer
  // as backgrounded.
  EXPECT_CALL(*main_thread_scheduler_, SetRendererHidden(true)).Times(0);
  EXPECT_CALL(*main_thread_scheduler_, SetRendererBackgrounded(true));
  EXPECT_CALL(*main_thread_scheduler_, SetRendererHidden(false)).Times(0);
  EXPECT_CALL(*main_thread_scheduler_, SetRendererBackgrounded(false)).Times(0);
  SetBackgroundState(base::Process::Priority::kBestEffort);
  testing::Mock::VerifyAndClear(main_thread_scheduler_);

  testing::Mock::AllowLeak(main_thread_scheduler_);
}

TEST_F(RenderThreadImplBrowserTest, RendererStateTransitionForegrounded) {
  // Going from an unknown to a foregrounded state should mark the renderer as
  // foregrounded and visible.
  EXPECT_CALL(*main_thread_scheduler_, SetRendererBackgrounded(false));
  EXPECT_CALL(*main_thread_scheduler_, SetRendererHidden(true)).Times(0);
  EXPECT_CALL(*main_thread_scheduler_, SetRendererBackgrounded(true)).Times(0);
  EXPECT_CALL(*main_thread_scheduler_, SetRendererHidden(false));
  SetBackgroundState(base::Process::Priority::kUserBlocking);
  testing::Mock::VerifyAndClear(main_thread_scheduler_);

  testing::Mock::AllowLeak(main_thread_scheduler_);
}

TEST_F(RenderThreadImplBrowserTest, TransferSharedMemoryRegions) {
  using blink::performance_scenarios::Scope;
  using blink::performance_scenarios::ScopedReadOnlyScenarioMemory;

  auto time_memory = base::AtomicSharedMemory<base::TimeTicks>::Create();
  ASSERT_TRUE(time_memory.has_value());
  auto scenario_memory =
      blink::performance_scenarios::SharedScenarioState::Create();
  ASSERT_TRUE(scenario_memory.has_value());

  // No shared memory regions mapped by default.
  EXPECT_EQ(base::internal::GetSharedLastForegroundTimeForMetricsForTesting(),
            nullptr);
  EXPECT_EQ(ScopedReadOnlyScenarioMemory::GetMappingForTesting(
                Scope::kCurrentProcess),
            nullptr);
  EXPECT_EQ(ScopedReadOnlyScenarioMemory::GetMappingForTesting(Scope::kGlobal),
            nullptr);

  // Invalid handles should be accepted.
  thread_->TransferSharedMemoryRegions(base::ReadOnlySharedMemoryRegion(),
                                       base::ReadOnlySharedMemoryRegion(),
                                       base::ReadOnlySharedMemoryRegion());
  EXPECT_EQ(base::internal::GetSharedLastForegroundTimeForMetricsForTesting(),
            nullptr);
  EXPECT_EQ(ScopedReadOnlyScenarioMemory::GetMappingForTesting(
                Scope::kCurrentProcess),
            nullptr);
  EXPECT_EQ(ScopedReadOnlyScenarioMemory::GetMappingForTesting(Scope::kGlobal),
            nullptr);

  // SharedLastForegroundTimeForMetrics should never be overwritten after it's
  // set, in case a thread is accessing the memory as it's unmapped. Performance
  // scenario memory can be overwritten since the mapping is refcounted.
  thread_->TransferSharedMemoryRegions(time_memory->DuplicateReadOnlyRegion(),
                                       base::ReadOnlySharedMemoryRegion(),
                                       base::ReadOnlySharedMemoryRegion());
  const auto* last_foreground_time_ptr =
      base::internal::GetSharedLastForegroundTimeForMetricsForTesting();
  EXPECT_NE(last_foreground_time_ptr, nullptr);
  EXPECT_EQ(ScopedReadOnlyScenarioMemory::GetMappingForTesting(
                Scope::kCurrentProcess),
            nullptr);
  EXPECT_EQ(ScopedReadOnlyScenarioMemory::GetMappingForTesting(Scope::kGlobal),
            nullptr);

  thread_->TransferSharedMemoryRegions(
      time_memory->DuplicateReadOnlyRegion(),
      scenario_memory->DuplicateReadOnlyRegion(),
      base::ReadOnlySharedMemoryRegion());
  EXPECT_EQ(base::internal::GetSharedLastForegroundTimeForMetricsForTesting(),
            last_foreground_time_ptr);
  EXPECT_NE(ScopedReadOnlyScenarioMemory::GetMappingForTesting(
                Scope::kCurrentProcess),
            nullptr);
  EXPECT_EQ(ScopedReadOnlyScenarioMemory::GetMappingForTesting(Scope::kGlobal),
            nullptr);

  thread_->TransferSharedMemoryRegions(
      time_memory->DuplicateReadOnlyRegion(),
      scenario_memory->DuplicateReadOnlyRegion(),
      scenario_memory->DuplicateReadOnlyRegion());
  EXPECT_EQ(base::internal::GetSharedLastForegroundTimeForMetricsForTesting(),
            last_foreground_time_ptr);
  EXPECT_NE(ScopedReadOnlyScenarioMemory::GetMappingForTesting(
                Scope::kCurrentProcess),
            nullptr);
  EXPECT_NE(ScopedReadOnlyScenarioMemory::GetMappingForTesting(Scope::kGlobal),
            nullptr);

  thread_->TransferSharedMemoryRegions(base::ReadOnlySharedMemoryRegion(),
                                       base::ReadOnlySharedMemoryRegion(),
                                       base::ReadOnlySharedMemoryRegion());
  EXPECT_EQ(base::internal::GetSharedLastForegroundTimeForMetricsForTesting(),
            last_foreground_time_ptr);
  EXPECT_EQ(ScopedReadOnlyScenarioMemory::GetMappingForTesting(
                Scope::kCurrentProcess),
            nullptr);
  EXPECT_EQ(ScopedReadOnlyScenarioMemory::GetMappingForTesting(Scope::kGlobal),
            nullptr);
}

}  // namespace content
