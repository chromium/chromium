// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/client_process_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/tracing/trace_test_utils.h"
#include "base/trace_event/memory_dump_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/tracing_observer_proto.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_instrumentation {

namespace {

using RawOSMemDumpMap = base::flat_map<base::ProcessId, mojom::RawOSMemDumpPtr>;

}  // namespace

// Exercises ClientProcessImpl::RequestOSMemoryDump against a real
// ClientProcessImpl instance, collecting OS memory metrics for the current
// process.
class ClientProcessImplTest : public testing::Test {
 public:
  ClientProcessImplTest() {
    // ClientProcessImpl's constructor touches TracingObserverProto, whose
    // Perfetto data source registration requires Perfetto tracing to be
    // initialized; |tracing_environment_| stands that up before this body runs.
    tracing::PerfettoTracedProcess::DataSourceBase::ResetTaskRunner(
        task_environment_.GetMainThreadTaskRunner());
    TracingObserverProto::GetInstance()->ResetForTesting();

    base::trace_event::MemoryDumpManager::GetInstance()
        ->set_dumper_registrations_ignored_for_testing(true);

    // The Coordinator pipe is required by the constructor but unused by the OS
    // dump path; keep the receiver end alive so the remote stays connected.
    mojo::PendingRemote<mojom::Coordinator> coordinator;
    coordinator_receiver_ = coordinator.InitWithNewPipeAndPassReceiver();

    // We don't need to keep the ClientProcess PendingRemote: the tests invoke
    // the overridden methods on |client_process_| directly, so the remote can
    // safely go out of scope here.
    mojo::PendingRemote<mojom::ClientProcess> process;
    auto process_receiver = process.InitWithNewPipeAndPassReceiver();

    client_process_ = base::WrapUnique(new ClientProcessImpl(
        std::move(process_receiver), std::move(coordinator),
        /*is_browser_process=*/true,
        /*initialize_memory_instrumentation=*/false));
  }

  ~ClientProcessImplTest() override {
    base::trace_event::MemoryDumpManager::GetInstance()->ResetForTesting();
  }

 protected:
  // Requests an OS memory dump for |pids| and blocks until it completes,
  // returning the outcome and populating |dumps| with the resulting map.
  mojom::RequestOutcome RequestOSMemoryDumpAndWait(
      const std::vector<base::ProcessId>& pids,
      RawOSMemDumpMap* dumps) {
    base::test::TestFuture<mojom::RequestOutcome, RawOSMemDumpMap> future;
    client_process_->RequestOSMemoryDump(mojom::MemoryMapOption::NONE,
                                         /*flags=*/{}, pids,
                                         future.GetCallback());
    auto result = future.Take();
    if (dumps) {
      *dumps = std::move(std::get<RawOSMemDumpMap>(result));
    }
    return std::get<mojom::RequestOutcome>(result);
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::TracingEnvironment tracing_environment_;
  mojo::PendingReceiver<mojom::Coordinator> coordinator_receiver_;
  std::unique_ptr<ClientProcessImpl> client_process_;
};

// The kNullProcessId pid is the "dump my own process" request used by every
// non-Linux/ChromeOS platform (see RequestOSMemoryDump in the mojom). It maps
// to base::Process::Current() internally.
TEST_F(ClientProcessImplTest, DumpsOwnProcessForNullPid) {
  RawOSMemDumpMap dumps;
  mojom::RequestOutcome outcome =
      RequestOSMemoryDumpAndWait({base::kNullProcessId}, &dumps);

  EXPECT_EQ(outcome, mojom::RequestOutcome::kSuccess);
  ASSERT_EQ(dumps.size(), 1u);

  auto it = dumps.find(base::kNullProcessId);
  ASSERT_NE(it, dumps.end());
  const mojom::RawOSMemDumpPtr& dump = it->second;
  ASSERT_TRUE(dump);
  EXPECT_TRUE(dump->platform_private_footprint);
  // A live process always has a non-zero resident set, so a zero value here
  // would indicate the dump was never actually populated.
  EXPECT_GT(dump->resident_set_kb, 0u);
}

// Passing an explicit, real pid exercises the base::Process::Open() path that
// Linux/ChromeOS use when the browser dumps other processes. Here we target the
// current process so the syscalls succeed on every platform.
TEST_F(ClientProcessImplTest, DumpsProcessForRealPid) {
  const base::ProcessId pid = base::Process::Current().Pid();

  RawOSMemDumpMap dumps;
  mojom::RequestOutcome outcome = RequestOSMemoryDumpAndWait({pid}, &dumps);

  EXPECT_EQ(outcome, mojom::RequestOutcome::kSuccess);
  ASSERT_EQ(dumps.size(), 1u);

  auto it = dumps.find(pid);
  ASSERT_NE(it, dumps.end());
  const mojom::RawOSMemDumpPtr& dump = it->second;
  ASSERT_TRUE(dump);
  EXPECT_TRUE(dump->platform_private_footprint);
  EXPECT_GT(dump->resident_set_kb, 0u);
}

}  // namespace memory_instrumentation
