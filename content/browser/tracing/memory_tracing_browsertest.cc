// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/trace_config_memory_test_util.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::trace_event::MemoryDumpArgs;
using base::trace_event::MemoryDumpDeterminism;
using base::trace_event::MemoryDumpLevelOfDetail;
using base::trace_event::MemoryDumpManager;
using base::trace_event::MemoryDumpType;
using base::trace_event::ProcessMemoryDump;
using testing::_;
using testing::Return;

namespace content {

// A mock dump provider, used to check that dump requests actually end up
// creating memory dumps.
class MockDumpProvider : public base::trace_event::MemoryDumpProvider {
 public:
  MOCK_METHOD2(OnMemoryDump, bool(const MemoryDumpArgs& args,
                                  ProcessMemoryDump* pmd));
};

class MemoryTracingTest : public ContentBrowserTest {
 public:
  // Used as callback argument for MemoryDumpManager::RequestGlobalDump():
  void OnGlobalMemoryDumpDone(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::OnceClosure closure,
      uint32_t request_index,
      bool success,
      uint64_t dump_guid) {
    // Make sure we run the RunLoop closure on the same thread that originated
    // the run loop (which is the IN_PROC_BROWSER_TEST_F main thread).
    if (!task_runner->RunsTasksInCurrentSequence()) {
      task_runner->PostTask(
          FROM_HERE, base::BindOnce(&MemoryTracingTest::OnGlobalMemoryDumpDone,
                                    base::Unretained(this), task_runner,
                                    std::move(closure), request_index, success,
                                    dump_guid));
      return;
    }
    if (success)
      EXPECT_NE(0u, dump_guid);
    OnMemoryDumpDone(request_index, success);
    if (closure)
      std::move(closure).Run();
  }

  void RequestGlobalDumpWithClosure(
      bool from_renderer_thread,
      const MemoryDumpType& dump_type,
      const MemoryDumpLevelOfDetail& level_of_detail,
      base::OnceClosure closure) {
    uint32_t request_index = next_request_index_++;
    auto callback = base::BindOnce(
        &MemoryTracingTest::OnGlobalMemoryDumpDone, base::Unretained(this),
        base::SingleThreadTaskRunner::GetCurrentDefault(), std::move(closure),
        request_index);
    if (from_renderer_thread) {
      PostTaskToInProcessRendererAndWait(base::BindOnce(
          &memory_instrumentation::MemoryInstrumentation::
              RequestGlobalDumpAndAppendToTrace,
          base::Unretained(
              memory_instrumentation::MemoryInstrumentation::GetInstance()),
          dump_type, level_of_detail, MemoryDumpDeterminism::kNone,
          std::move(callback)));
    } else {
      memory_instrumentation::MemoryInstrumentation::GetInstance()
          ->RequestGlobalDumpAndAppendToTrace(dump_type, level_of_detail,
                                              MemoryDumpDeterminism::kNone,
                                              std::move(callback));
    }
  }

 protected:
  void SetUp() override {
    next_request_index_ = 0;

    mock_dump_provider_ = std::make_unique<MockDumpProvider>();
    MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        mock_dump_provider_.get(), "MockDumpProvider", nullptr);
    MemoryDumpManager::GetInstance()
        ->set_dumper_registrations_ignored_for_testing(false);
    ContentBrowserTest::SetUp();
  }

  void TearDown() override {
    MemoryDumpManager::GetInstance()->UnregisterAndDeleteDumpProviderSoon(
        std::move(mock_dump_provider_));
    mock_dump_provider_.reset();
    ContentBrowserTest::TearDown();
  }

  void EnableMemoryTracing() {
    // Re-enabling tracing could crash these tests https://crbug.com/657628 .
    if (base::trace_event::TraceLog::GetInstance()->IsEnabled()) {
      FAIL() << "Tracing seems to be already enabled. "
                "Very likely this is because the startup tracing file "
                "has been leaked from a previous test.";
    }
    // Enable tracing without periodic dumps.
    base::trace_event::TraceConfig trace_config(
        base::trace_event::TraceConfigMemoryTestUtil::
            GetTraceConfig_EmptyTriggers());

    base::RunLoop run_loop;
    bool success = TracingController::GetInstance()->StartTracing(
      trace_config, run_loop.QuitClosure());
    EXPECT_TRUE(success);
    run_loop.Run();
  }

  void DisableTracing() {
    base::RunLoop run_loop;
    bool success = TracingController::GetInstance()->StopTracing(
        TracingControllerImpl::CreateCallbackEndpoint(base::BindOnce(
            [](base::OnceClosure quit_closure,
               std::unique_ptr<std::string> trace_str) {
              std::move(quit_closure).Run();
            },
            run_loop.QuitClosure())));
    EXPECT_TRUE(success);
    run_loop.Run();
  }

  void RequestGlobalDumpAndWait(
      bool from_renderer_thread,
      const MemoryDumpType& dump_type,
      const MemoryDumpLevelOfDetail& level_of_detail) {
    base::RunLoop run_loop;
    RequestGlobalDumpWithClosure(from_renderer_thread, dump_type,
                                 level_of_detail, run_loop.QuitClosure());
    run_loop.Run();
  }

  void RequestGlobalDump(bool from_renderer_thread,
                         const MemoryDumpType& dump_type,
                         const MemoryDumpLevelOfDetail& level_of_detail) {
    RequestGlobalDumpWithClosure(from_renderer_thread, dump_type,
                                 level_of_detail, base::NullCallback());
  }

  void Navigate(Shell* shell) {
    EXPECT_TRUE(NavigateToURL(shell, GetTestUrl("", "title1.html")));
  }

  MOCK_METHOD2(OnMemoryDumpDone, void(uint32_t request_index, bool successful));

  std::unique_ptr<MockDumpProvider> mock_dump_provider_;
  uint32_t next_request_index_;
  bool last_callback_success_;
};

// Run SingleProcessMemoryTracingTests only on Android, since these tests are
// intended to give coverage to Android WebView.
#if BUILDFLAG(IS_ANDROID)

class SingleProcessMemoryTracingTest : public MemoryTracingTest {
 public:
  SingleProcessMemoryTracingTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kSingleProcess);
  }
};

// https://crbug.com/788788
#if BUILDFLAG(IS_ANDROID) && defined(ADDRESS_SANITIZER)
#define MAYBE_BrowserInitiatedSingleDump DISABLED_BrowserInitiatedSingleDump
#else
#define MAYBE_BrowserInitiatedSingleDump BrowserInitiatedSingleDump
#endif  // BUILDFLAG(IS_ANDROID) && defined(ADDRESS_SANITIZER)

// Checks that a memory dump initiated from a the main browser thread ends up in
// a single dump even in single process mode.
IN_PROC_BROWSER_TEST_F(SingleProcessMemoryTracingTest,
                       MAYBE_BrowserInitiatedSingleDump) {
  Navigate(shell());

  EXPECT_CALL(*mock_dump_provider_, OnMemoryDump(_,_)).WillOnce(Return(true));
  EXPECT_CALL(*this, OnMemoryDumpDone(_, true /* success */));

  EnableMemoryTracing();
  RequestGlobalDumpAndWait(false /* from_renderer_thread */,
                           MemoryDumpType::kExplicitlyTriggered,
                           MemoryDumpLevelOfDetail::kDetailed);
  DisableTracing();
}

// https://crbug.com/788788
#if BUILDFLAG(IS_ANDROID) && defined(ADDRESS_SANITIZER)
#define MAYBE_RendererInitiatedSingleDump DISABLED_RendererInitiatedSingleDump
#else
#define MAYBE_RendererInitiatedSingleDump RendererInitiatedSingleDump
#endif  // BUILDFLAG(IS_ANDROID) && defined(ADDRESS_SANITIZER)

// Checks that a memory dump initiated from a renderer thread ends up in a
// single dump even in single process mode.
IN_PROC_BROWSER_TEST_F(SingleProcessMemoryTracingTest,
                       MAYBE_RendererInitiatedSingleDump) {
  Navigate(shell());

  EXPECT_CALL(*mock_dump_provider_, OnMemoryDump(_,_)).WillOnce(Return(true));
  EXPECT_CALL(*this, OnMemoryDumpDone(_, true /* success */));

  EnableMemoryTracing();
  RequestGlobalDumpAndWait(true /* from_renderer_thread */,
                           MemoryDumpType::kExplicitlyTriggered,
                           MemoryDumpLevelOfDetail::kDetailed);
  DisableTracing();
}

// https://crbug.com/788788
#if BUILDFLAG(IS_ANDROID) && defined(ADDRESS_SANITIZER)
#define MAYBE_ManyInterleavedDumps DISABLED_ManyInterleavedDumps
#else
#define MAYBE_ManyInterleavedDumps ManyInterleavedDumps
#endif  // BUILDFLAG(IS_ANDROID) && defined(ADDRESS_SANITIZER)
IN_PROC_BROWSER_TEST_F(SingleProcessMemoryTracingTest,
                       MAYBE_ManyInterleavedDumps) {
  Navigate(shell());

  EXPECT_CALL(*mock_dump_provider_, OnMemoryDump(_,_))
      .Times(4)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*this, OnMemoryDumpDone(_, true /* success */)).Times(4);

  EnableMemoryTracing();
  RequestGlobalDumpAndWait(true /* from_renderer_thread */,
                           MemoryDumpType::kExplicitlyTriggered,
                           MemoryDumpLevelOfDetail::kDetailed);
  RequestGlobalDumpAndWait(false /* from_renderer_thread */,
                           MemoryDumpType::kExplicitlyTriggered,
                           MemoryDumpLevelOfDetail::kDetailed);
  RequestGlobalDumpAndWait(false /* from_renderer_thread */,
                           MemoryDumpType::kExplicitlyTriggered,
                           MemoryDumpLevelOfDetail::kDetailed);
  RequestGlobalDumpAndWait(true /* from_renderer_thread */,
                           MemoryDumpType::kExplicitlyTriggered,
                           MemoryDumpLevelOfDetail::kDetailed);
  DisableTracing();
}

// Checks that, if there already is a memory dump in progress, subsequent memory
// dump requests are queued and carried out after it's finished. Also checks
// that periodic dump requests fail in case there is already a request in the
// queue with the same level of detail.
// Flaky failures on all platforms. https://crbug.com/752613
IN_PROC_BROWSER_TEST_F(SingleProcessMemoryTracingTest, DISABLED_QueuedDumps) {
  Navigate(shell());

  EnableMemoryTracing();

  // Issue the following 6 global memory dump requests:
  //
  //   0 (ED)  req-------------------------------------->ok
  //   1 (PD)      req->fail(0)
  //   2 (PL)                   req------------------------>ok
  //   3 (PL)                       req->fail(2)
  //   4 (EL)                                    req---------->ok
  //   5 (ED)                                        req--------->ok
  //   6 (PL)                                                        req->ok
  //
  // where P=kPeriodicInterval, E=kExplicitlyTriggered, D=kDetailed and L=kLight.

  EXPECT_CALL(*mock_dump_provider_, OnMemoryDump(_, _))
      .Times(5)
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*this, OnMemoryDumpDone(0, true /* success */));
  RequestGlobalDump(true /* from_renderer_thread */,
                    MemoryDumpType::kExplicitlyTriggered,
                    MemoryDumpLevelOfDetail::kDetailed);

  // This dump should fail immediately because there's already a detailed dump
  // request in the queue.
  EXPECT_CALL(*this, OnMemoryDumpDone(1, false /* success */));
  RequestGlobalDump(true /* from_renderer_thread */,
                    MemoryDumpType::kPeriodicInterval,
                    MemoryDumpLevelOfDetail::kDetailed);

  EXPECT_CALL(*this, OnMemoryDumpDone(2, true /* success */));
  RequestGlobalDump(true /* from_renderer_thread */,
                    MemoryDumpType::kPeriodicInterval,
                    MemoryDumpLevelOfDetail::kLight);

  // This dump should fail immediately because there's already a light dump
  // request in the queue.
  EXPECT_CALL(*this, OnMemoryDumpDone(3, false /* success */));
  RequestGlobalDump(true /* from_renderer_thread */,
                    MemoryDumpType::kPeriodicInterval,
                    MemoryDumpLevelOfDetail::kLight);

  EXPECT_CALL(*this, OnMemoryDumpDone(4, true /* success */));
  RequestGlobalDump(true /* from_renderer_thread */,
                    MemoryDumpType::kExplicitlyTriggered,
                    MemoryDumpLevelOfDetail::kLight);

  EXPECT_CALL(*this, OnMemoryDumpDone(5, true /* success */));
  RequestGlobalDumpAndWait(true /* from_renderer_thread */,
                           MemoryDumpType::kExplicitlyTriggered,
                           MemoryDumpLevelOfDetail::kDetailed);

  EXPECT_CALL(*this, OnMemoryDumpDone(6, true /* success */));
  RequestGlobalDumpAndWait(true /* from_renderer_thread */,
                           MemoryDumpType::kPeriodicInterval,
                           MemoryDumpLevelOfDetail::kLight);

  DisableTracing();
}

#endif  // BUILDFLAG(IS_ANDROID)

// Flaky on Mac. crbug.com/809809
// Failing on Android ASAN. crbug.com/1041392
// TODO(crbug.com/40720107): OSMetrics::GetProcessMemoryMaps is not
// implemented on Fuchsia
#if BUILDFLAG(IS_MAC) ||                                     \
    (BUILDFLAG(IS_ANDROID) && defined(ADDRESS_SANITIZER)) || \
    BUILDFLAG(IS_FUCHSIA)
#define MAYBE_BrowserInitiatedDump DISABLED_BrowserInitiatedDump
#else
#define MAYBE_BrowserInitiatedDump BrowserInitiatedDump
#endif
// Checks that a memory dump initiated from a the main browser thread ends up in
// a successful dump.
IN_PROC_BROWSER_TEST_F(MemoryTracingTest, MAYBE_BrowserInitiatedDump) {
  Navigate(shell());

  EXPECT_CALL(*mock_dump_provider_, OnMemoryDump(_,_)).WillOnce(Return(true));
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // TODO(ssid): Test for dump success once the on start tracing done callback
  // is fixed to be called after enable tracing is acked by all processes,
  // crbug.com/709524. The test still tests if dumping does not crash.
  EXPECT_CALL(*this, OnMemoryDumpDone(_, _));
#else
  EXPECT_CALL(*this, OnMemoryDumpDone(_, true /* success */));
#endif

  EnableMemoryTracing();
  RequestGlobalDumpAndWait(false /* from_renderer_thread */,
                           MemoryDumpType::kExplicitlyTriggered,
                           MemoryDumpLevelOfDetail::kDetailed);
  DisableTracing();
}

}  // namespace content
