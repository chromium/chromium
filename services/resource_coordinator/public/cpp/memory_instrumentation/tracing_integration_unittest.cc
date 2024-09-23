// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/client_process_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_io_thread.h"
#include "base/test/trace_event_analyzer.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_manager_test_utils.h"
#include "base/trace_event/memory_dump_scheduler.h"
#include "base/trace_event/memory_infra_background_allowlist.h"
#include "base/trace_event/trace_buffer.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/trace_config_memory_test_util.h"
#include "base/trace_event/trace_log.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/tracing_observer_proto.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::Return;

using base::trace_event::MemoryAllocatorDump;
using base::trace_event::MemoryDumpArgs;
using base::trace_event::MemoryDumpDeterminism;
using base::trace_event::MemoryDumpLevelOfDetail;
using base::trace_event::MemoryDumpManager;
using base::trace_event::MemoryDumpProvider;
using base::trace_event::MemoryDumpRequestArgs;
using base::trace_event::MemoryDumpScheduler;
using base::trace_event::MemoryDumpType;
using base::trace_event::ProcessMemoryDump;
using base::trace_event::TraceConfig;
using base::trace_event::TraceLog;
using base::trace_event::TraceResultBuffer;

namespace memory_instrumentation {

namespace {

constexpr char kMDPName[] = "TestDumpProvider";
constexpr char kWhitelistedMDPName[] = "WhitelistedTestDumpProvider";
constexpr char kBackgroundButNotSummaryWhitelistedMDPName[] =
    "BackgroundButNotSummaryWhitelistedTestDumpProvider";
constexpr auto kTestMDPWhitelist = std::to_array<std::string_view>(
    {kWhitelistedMDPName, kBackgroundButNotSummaryWhitelistedMDPName});

// GTest matchers for MemoryDumpRequestArgs arguments.
MATCHER(IsDetailedDump, "") {
  return arg.level_of_detail == MemoryDumpLevelOfDetail::kDetailed;
}

MATCHER(IsLightDump, "") {
  return arg.level_of_detail == MemoryDumpLevelOfDetail::kLight;
}

MATCHER(IsBackgroundDump, "") {
  return arg.level_of_detail == MemoryDumpLevelOfDetail::kBackground;
}

// TODO(ssid): This class is replicated in memory_dump_manager_unittest. Move
// this to memory_dump_manager_test_utils.h crbug.com/728199.
class MockMemoryDumpProvider : public MemoryDumpProvider {
 public:
  MOCK_METHOD0(Destructor, void());
  MOCK_METHOD2(OnMemoryDump,
               bool(const MemoryDumpArgs& args, ProcessMemoryDump* pmd));

  MockMemoryDumpProvider() : enable_mock_destructor(false) {
    ON_CALL(*this, OnMemoryDump(_, _))
        .WillByDefault(
            Invoke([](const MemoryDumpArgs&, ProcessMemoryDump* pmd) -> bool {
              return true;
            }));
  }

  ~MockMemoryDumpProvider() override {
    if (enable_mock_destructor)
      Destructor();
  }

  bool enable_mock_destructor;
};

}  // namespace

class MemoryTracingIntegrationTest;

class MockCoordinator : public mojom::Coordinator {
 public:
  explicit MockCoordinator(MemoryTracingIntegrationTest* client)
      : client_(client) {}

  void BindReceiver(mojo::PendingReceiver<mojom::Coordinator> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  void RequestGlobalMemoryDump(
      MemoryDumpType dump_type,
      MemoryDumpLevelOfDetail level_of_detail,
      MemoryDumpDeterminism determinism,
      const std::vector<std::string>& allocator_dump_names,
      RequestGlobalMemoryDumpCallback) override;

  void RequestGlobalMemoryDumpForPid(
      base::ProcessId pid,
      const std::vector<std::string>& allocator_dump_names,
      RequestGlobalMemoryDumpForPidCallback) override {}

  void RequestPrivateMemoryFootprint(
      base::ProcessId pid,
      RequestPrivateMemoryFootprintCallback) override {}

  void RequestGlobalMemoryDumpAndAppendToTrace(
      MemoryDumpType dump_type,
      MemoryDumpLevelOfDetail level_of_detail,
      MemoryDumpDeterminism determinism,
      RequestGlobalMemoryDumpAndAppendToTraceCallback) override;

 private:
  mojo::ReceiverSet<mojom::Coordinator> receivers_;
  raw_ptr<MemoryTracingIntegrationTest> client_;
};

class MemoryTracingIntegrationTest : public testing::Test {
 public:
  void SetUp() override {
    task_environment_ =
        std::make_unique<base::test::SingleThreadTaskEnvironment>();
    coordinator_ = std::make_unique<MockCoordinator>(this);

    TraceLog::GetInstance()->InitializePerfettoIfNeeded();
    tracing::PerfettoTracedProcess::GetTaskRunner()->ResetTaskRunnerForTesting(
        base::SingleThreadTaskRunner::GetCurrentDefault());

    TracingObserverProto::GetInstance()->ResetForTesting();
  }

  void InitializeClientProcess(mojom::ProcessType process_type) {
    mdm_ = MemoryDumpManager::GetInstance();
    mdm_->set_dumper_registrations_ignored_for_testing(true);

    mojo::PendingRemote<mojom::Coordinator> coordinator;
    mojo::PendingRemote<mojom::ClientProcess> process;
    auto process_receiver = process.InitWithNewPipeAndPassReceiver();
    coordinator_->BindReceiver(coordinator.InitWithNewPipeAndPassReceiver());
    client_process_.reset(new ClientProcessImpl(
        std::move(process_receiver), std::move(coordinator),
        process_type == mojom::ProcessType::BROWSER,
        /*initialize_memory_instrumentation=*/false));
  }

  void TearDown() override {
    TraceLog::GetInstance()->SetDisabled();
    mdm_->ResetForTesting();
    mdm_ = nullptr;
    client_process_.reset();
    coordinator_.reset();
    task_environment_.reset();
    TraceLog::ResetForTesting();
  }

  // Blocks the current thread (spinning a nested message loop) until the
  // memory dump is complete. Returns:
  // - return value: the |success| from the RequestChromeMemoryDump() callback.
  bool RequestChromeDumpAndWait(
      MemoryDumpType dump_type,
      MemoryDumpLevelOfDetail level_of_detail,
      std::unique_ptr<base::trace_event::ProcessMemoryDump>* result = nullptr) {
    base::RunLoop run_loop;
    bool success = false;
    uint64_t req_guid = ++guid_counter_;
    MemoryDumpRequestArgs request_args{req_guid, dump_type, level_of_detail};
    ClientProcessImpl::RequestChromeMemoryDumpCallback callback =
        base::BindOnce(
            [](bool* curried_success, base::OnceClosure curried_quit_closure,
               std::unique_ptr<base::trace_event::ProcessMemoryDump>*
                   curried_result,
               uint64_t curried_expected_guid, bool success, uint64_t dump_guid,
               std::unique_ptr<base::trace_event::ProcessMemoryDump> result) {
              EXPECT_EQ(curried_expected_guid, dump_guid);
              *curried_success = success;
              if (curried_result)
                *curried_result = std::move(result);
              std::move(curried_quit_closure).Run();
            },
            &success, run_loop.QuitClosure(), result, req_guid);
    client_process_->RequestChromeMemoryDump(request_args, std::move(callback));
    run_loop.Run();
    return success;
  }

  void RequestChromeDump(MemoryDumpType dump_type,
                         MemoryDumpLevelOfDetail level_of_detail,
                         MemoryDumpDeterminism determinism) {
    uint64_t req_guid = ++guid_counter_;
    MemoryDumpRequestArgs request_args{req_guid, dump_type, level_of_detail,
                                       determinism};
    ClientProcessImpl::RequestChromeMemoryDumpCallback callback =
        base::BindOnce(
            [](bool success, uint64_t dump_guid,
               std::unique_ptr<base::trace_event::ProcessMemoryDump> result) {
            });
    client_process_->RequestChromeMemoryDump(request_args, std::move(callback));
  }

 protected:
  void EnableMemoryInfraTracing() {
    TraceLog::GetInstance()->SetEnabled(
        TraceConfig(MemoryDumpManager::kTraceCategory, ""),
        TraceLog::RECORDING_MODE);
  }

  void EnableMemoryInfraTracingWithTraceConfig(
      const std::string& trace_config) {
    TraceLog::GetInstance()->SetEnabled(TraceConfig(trace_config),
                                        TraceLog::RECORDING_MODE);
  }

  void DisableTracing() { TraceLog::GetInstance()->SetDisabled(); }

  void RegisterDumpProvider(
      MemoryDumpProvider* mdp,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const MemoryDumpProvider::Options& options,
      const char* name = kMDPName) {
    mdm_->set_dumper_registrations_ignored_for_testing(false);
    mdm_->RegisterDumpProvider(mdp, name, std::move(task_runner), options);
    mdm_->set_dumper_registrations_ignored_for_testing(true);
  }

  void RegisterDumpProvider(
      MemoryDumpProvider* mdp,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    RegisterDumpProvider(mdp, task_runner, MemoryDumpProvider::Options());
  }

  bool IsPeriodicDumpingEnabled() const {
    return MemoryDumpScheduler::GetInstance()->is_enabled_for_testing();
  }

  raw_ptr<MemoryDumpManager> mdm_;

 private:
  std::unique_ptr<base::test::SingleThreadTaskEnvironment> task_environment_;
  std::unique_ptr<MockCoordinator> coordinator_;
  std::unique_ptr<ClientProcessImpl> client_process_;
  uint64_t guid_counter_ = 0;
};

void MockCoordinator::RequestGlobalMemoryDump(
    MemoryDumpType dump_type,
    MemoryDumpLevelOfDetail level_of_detail,
    MemoryDumpDeterminism determinism,
    const std::vector<std::string>& allocator_dump_names,
    RequestGlobalMemoryDumpCallback callback) {
  client_->RequestChromeDump(dump_type, level_of_detail, determinism);
  std::move(callback).Run(true, mojom::GlobalMemoryDumpPtr());
}

void MockCoordinator::RequestGlobalMemoryDumpAndAppendToTrace(
    MemoryDumpType dump_type,
    MemoryDumpLevelOfDetail level_of_detail,
    MemoryDumpDeterminism determinism,
    RequestGlobalMemoryDumpAndAppendToTraceCallback callback) {
  client_->RequestChromeDump(dump_type, level_of_detail, determinism);
  std::move(callback).Run(true, true);
}

// Checks that is the ClientProcessImpl is initialized after tracing already
// began, it will still late-join the party (real use case: startup tracing).
TEST_F(MemoryTracingIntegrationTest, InitializedAfterStartOfTracing) {
  EnableMemoryInfraTracing();

  // TODO(ssid): Add tests for
  // MemoryInstrumentation::RequestGlobalDumpAndAppendToTrace to fail gracefully
  // before creating ClientProcessImpl.

  // Now late-initialize and check that the CreateProcessDump() completes
  // successfully.
  InitializeClientProcess(mojom::ProcessType::RENDERER);
  MockMemoryDumpProvider mdp;
  RegisterDumpProvider(&mdp, nullptr, MemoryDumpProvider::Options());
  EXPECT_CALL(mdp, OnMemoryDump(_, _)).Times(1);
  EXPECT_TRUE(RequestChromeDumpAndWait(MemoryDumpType::kExplicitlyTriggered,
                                       MemoryDumpLevelOfDetail::kDetailed));
  DisableTracing();
}

// Configures periodic dumps with MemoryDumpLevelOfDetail::BACKGROUND triggers
// and tests that only BACKGROUND are added to the trace, but not LIGHT or
// DETAILED, even if requested explicitly.
TEST_F(MemoryTracingIntegrationTest, TestBackgroundTracingSetup) {
  InitializeClientProcess(mojom::ProcessType::BROWSER);
  base::trace_event::SetDumpProviderAllowlistForTesting(kTestMDPWhitelist);
  auto mdp = std::make_unique<MockMemoryDumpProvider>();
  RegisterDumpProvider(&*mdp, nullptr, MemoryDumpProvider::Options(),
                       kWhitelistedMDPName);

  base::RunLoop run_loop;
  auto test_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  auto quit_closure = run_loop.QuitClosure();

  {
    testing::InSequence sequence;
    EXPECT_CALL(*mdp, OnMemoryDump(IsBackgroundDump(), _))
        .Times(3)
        .WillRepeatedly(Invoke(
            [](const MemoryDumpArgs&, ProcessMemoryDump*) { return true; }));
    EXPECT_CALL(*mdp, OnMemoryDump(IsBackgroundDump(), _))
        .WillOnce(Invoke([test_task_runner, quit_closure](const MemoryDumpArgs&,
                                                          ProcessMemoryDump*) {
          test_task_runner->PostTask(FROM_HERE, quit_closure);
          return true;
        }));
    EXPECT_CALL(*mdp, OnMemoryDump(IsBackgroundDump(), _)).Times(AnyNumber());
  }

  EnableMemoryInfraTracingWithTraceConfig(
      base::trace_event::TraceConfigMemoryTestUtil::
          GetTraceConfig_BackgroundTrigger(1 /* period_ms */));

  run_loop.Run();

  // When requesting non-BACKGROUND dumps the MDP will be invoked.
  EXPECT_CALL(*mdp, OnMemoryDump(IsLightDump(), _));
  EXPECT_TRUE(RequestChromeDumpAndWait(MemoryDumpType::kExplicitlyTriggered,
                                       MemoryDumpLevelOfDetail::kLight));

  EXPECT_CALL(*mdp, OnMemoryDump(IsDetailedDump(), _));
  EXPECT_TRUE(RequestChromeDumpAndWait(MemoryDumpType::kExplicitlyTriggered,
                                       MemoryDumpLevelOfDetail::kDetailed));

  ASSERT_TRUE(IsPeriodicDumpingEnabled());
  DisableTracing();
  mdm_->UnregisterAndDeleteDumpProviderSoon(std::move(mdp));
}

// This test (and the TraceConfigExpectationsWhenIsCoordinator below)
// crystallizes the expectations of the chrome://tracing UI and chrome telemetry
// w.r.t. periodic dumps in memory-infra, handling gracefully the transition
// between the legacy and the new-style (JSON-based) TraceConfig.
TEST_F(MemoryTracingIntegrationTest, TraceConfigExpectations) {
  InitializeClientProcess(mojom::ProcessType::RENDERER);

  // We don't need to create any dump in this test, only check whether the dumps
  // are requested or not.

  // Enabling memory-infra in a non-coordinator process should not trigger any
  // periodic dumps.
  EnableMemoryInfraTracing();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(IsPeriodicDumpingEnabled());
  DisableTracing();

  // Enabling memory-infra with the new (JSON) TraceConfig in a non-coordinator
  // process with a fully defined trigger config should NOT enable any periodic
  // dumps.
  EnableMemoryInfraTracingWithTraceConfig(
      base::trace_event::TraceConfigMemoryTestUtil::
          GetTraceConfig_PeriodicTriggers(1, 5));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(IsPeriodicDumpingEnabled());
  DisableTracing();
}

TEST_F(MemoryTracingIntegrationTest, TraceConfigExpectationsWhenIsCoordinator) {
  InitializeClientProcess(mojom::ProcessType::BROWSER);

  // Enabling memory-infra with the legacy TraceConfig (category filter) in
  // a coordinator process should not enable periodic dumps.
  EnableMemoryInfraTracing();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(IsPeriodicDumpingEnabled());
  DisableTracing();

  // Enabling memory-infra with the new (JSON) TraceConfig in a coordinator
  // process while specifying a "memory_dump_config" section should enable
  // periodic dumps. This is to preserve the behavior chrome://tracing UI, that
  // is: ticking memory-infra should dump periodically with an explicit config.
  EnableMemoryInfraTracingWithTraceConfig(
      base::trace_event::TraceConfigMemoryTestUtil::
          GetTraceConfig_PeriodicTriggers(100, 5));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsPeriodicDumpingEnabled());
  DisableTracing();

  // Enabling memory-infra with the new (JSON) TraceConfig in a coordinator
  // process with an empty "memory_dump_config" should NOT enable periodic
  // dumps. This is the way telemetry is supposed to use memory-infra with
  // only explicitly triggered dumps.
  EnableMemoryInfraTracingWithTraceConfig(
      base::trace_event::TraceConfigMemoryTestUtil::
          GetTraceConfig_EmptyTriggers());
  EXPECT_FALSE(IsPeriodicDumpingEnabled());
  DisableTracing();
}

TEST_F(MemoryTracingIntegrationTest, PeriodicDumpingWithMultipleModes) {
  InitializeClientProcess(mojom::ProcessType::BROWSER);

  // Enabling memory-infra with the new (JSON) TraceConfig in a coordinator
  // process with a fully defined trigger config should cause periodic dumps to
  // be performed in the correct order.
  base::RunLoop run_loop;
  auto test_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  auto quit_closure = run_loop.QuitClosure();

  const int kHeavyDumpRate = 5;
  const int kLightDumpPeriodMs = 1;
  const int kHeavyDumpPeriodMs = kHeavyDumpRate * kLightDumpPeriodMs;

  // The expected sequence with light=1ms, heavy=5ms is H,L,L,L,L,H,...
  auto mdp = std::make_unique<MockMemoryDumpProvider>();
  RegisterDumpProvider(&*mdp, nullptr, MemoryDumpProvider::Options(),
                       kWhitelistedMDPName);

  testing::InSequence sequence;
  EXPECT_CALL(*mdp, OnMemoryDump(IsDetailedDump(), _));
  EXPECT_CALL(*mdp, OnMemoryDump(IsLightDump(), _)).Times(kHeavyDumpRate - 1);
  EXPECT_CALL(*mdp, OnMemoryDump(IsDetailedDump(), _));
  EXPECT_CALL(*mdp, OnMemoryDump(IsLightDump(), _)).Times(kHeavyDumpRate - 2);
  EXPECT_CALL(*mdp, OnMemoryDump(IsLightDump(), _))
      .WillOnce(Invoke([test_task_runner, quit_closure](const MemoryDumpArgs&,
                                                        ProcessMemoryDump*) {
        test_task_runner->PostTask(FROM_HERE, quit_closure);
        return true;
      }));

  // Swallow all the final spurious calls until tracing gets disabled.
  EXPECT_CALL(*mdp, OnMemoryDump(_, _)).Times(AnyNumber());

  EnableMemoryInfraTracingWithTraceConfig(
      base::trace_event::TraceConfigMemoryTestUtil::
          GetTraceConfig_PeriodicTriggers(kLightDumpPeriodMs,
                                          kHeavyDumpPeriodMs));
  run_loop.Run();
  DisableTracing();
  mdm_->UnregisterAndDeleteDumpProviderSoon(std::move(mdp));
}

TEST_F(MemoryTracingIntegrationTest, TestWhitelistingMDP) {
  InitializeClientProcess(mojom::ProcessType::RENDERER);
  base::trace_event::SetDumpProviderAllowlistForTesting(kTestMDPWhitelist);
  std::unique_ptr<MockMemoryDumpProvider> mdp1(new MockMemoryDumpProvider);
  RegisterDumpProvider(mdp1.get(), nullptr);
  std::unique_ptr<MockMemoryDumpProvider> mdp2(new MockMemoryDumpProvider);
  RegisterDumpProvider(mdp2.get(), nullptr, MemoryDumpProvider::Options(),
                       kWhitelistedMDPName);

  EXPECT_CALL(*mdp1, OnMemoryDump(_, _)).Times(0);
  EXPECT_CALL(*mdp2, OnMemoryDump(_, _)).Times(1);

  EnableMemoryInfraTracing();
  EXPECT_FALSE(IsPeriodicDumpingEnabled());
  EXPECT_TRUE(RequestChromeDumpAndWait(MemoryDumpType::kExplicitlyTriggered,
                                       MemoryDumpLevelOfDetail::kBackground));
  DisableTracing();
}

// Regression test for https://crbug.com/766274 .
TEST_F(MemoryTracingIntegrationTest, GenerationChangeDoesntReenterMDM) {
  InitializeClientProcess(mojom::ProcessType::RENDERER);

  // We want the ThreadLocalEventBuffer MDPs to auto-register to repro this bug.
  mdm_->set_dumper_registrations_ignored_for_testing(false);

  // Disable any other tracing category, so we are likely to hit the
  // ThreadLocalEventBuffer in MemoryDumpManager::InbokeOnMemoryDump() first.
  const std::string kMemoryInfraTracingOnly =
      std::string("-*,") + MemoryDumpManager::kTraceCategory;

  auto thread =
      std::make_unique<base::TestIOThread>(base::TestIOThread::kAutoStart);

  TraceLog::GetInstance()->SetEnabled(
      TraceConfig(kMemoryInfraTracingOnly,
                  base::trace_event::RECORD_UNTIL_FULL),
      TraceLog::RECORDING_MODE);

  // Creating a new thread after tracing has started causes the posted
  // TRACE_EVENT0 to initialize and register a new ThreadLocalEventBuffer.
  base::RunLoop run_loop;
  thread->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<base::SequencedTaskRunner> main_task_runner,
             base::OnceClosure quit_closure) {
            TRACE_EVENT0(MemoryDumpManager::kTraceCategory, "foo");
            main_task_runner->PostTask(FROM_HERE, std::move(quit_closure));
          },
          base::SequencedTaskRunner::GetCurrentDefault(),
          run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_TRUE(RequestChromeDumpAndWait(MemoryDumpType::kExplicitlyTriggered,
                                       MemoryDumpLevelOfDetail::kDetailed));
  DisableTracing();

  // Now enable tracing again with a different RECORD_ mode. This will cause
  // a TraceLog generation change. The generation change will be lazily detected
  // in the |thread|'s ThreadLocalEventBuffer on its next TRACE_EVENT call (or
  // whatever ends up calling InitializeThreadLocalEventBufferIfSupported()).
  // The bug here conisted in MemoryDumpManager::InvokeOnMemoryDump() to hit
  // that (which in turn causes an invalidation of the ThreadLocalEventBuffer)
  // after having checked that the MDP is valid and having decided to invoke it.
  TraceLog::GetInstance()->SetEnabled(
      TraceConfig(kMemoryInfraTracingOnly,
                  base::trace_event::RECORD_CONTINUOUSLY),
      TraceLog::RECORDING_MODE);
  EXPECT_TRUE(RequestChromeDumpAndWait(MemoryDumpType::kExplicitlyTriggered,
                                       MemoryDumpLevelOfDetail::kDetailed));
  DisableTracing();
}

}  // namespace memory_instrumentation
