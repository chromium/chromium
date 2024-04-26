// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/coordinator_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/trace_test_utils.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "build/build_config.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/tracing_observer_proto.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "services/tracing/public/cpp/perfetto/producer_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceClosure;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

using GetVmRegionsForHeapProfilerCallback = memory_instrumentation::
    CoordinatorImpl::GetVmRegionsForHeapProfilerCallback;
using RequestGlobalMemoryDumpAndAppendToTraceCallback = memory_instrumentation::
    CoordinatorImpl::RequestGlobalMemoryDumpAndAppendToTraceCallback;
using RequestGlobalMemoryDumpCallback =
    memory_instrumentation::CoordinatorImpl::RequestGlobalMemoryDumpCallback;
using RequestGlobalMemoryDumpForPidCallback = memory_instrumentation::
    CoordinatorImpl::RequestGlobalMemoryDumpForPidCallback;
using base::trace_event::MemoryAllocatorDump;
using base::trace_event::MemoryDumpArgs;
using base::trace_event::MemoryDumpLevelOfDetail;
using base::trace_event::MemoryDumpManager;
using base::trace_event::MemoryDumpRequestArgs;
using base::trace_event::MemoryDumpType;
using base::trace_event::ProcessMemoryDump;
using base::trace_event::TraceLog;
using memory_instrumentation::mojom::GlobalMemoryDump;
using memory_instrumentation::mojom::GlobalMemoryDumpPtr;

namespace memory_instrumentation {

class FakeCoordinatorImpl : public CoordinatorImpl {
 public:
  FakeCoordinatorImpl() = default;
  ~FakeCoordinatorImpl() override = default;

  MOCK_CONST_METHOD0(ComputePidToServiceNamesMap,
                     std::map<base::ProcessId, std::vector<std::string>>());
};

class CoordinatorImplTest : public testing::Test {
 public:
  CoordinatorImplTest() = default;

  void SetUp() override {
    coordinator_ = std::make_unique<NiceMock<FakeCoordinatorImpl>>();
    tracing::PerfettoTracedProcess::GetTaskRunner()->ResetTaskRunnerForTesting(
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }

  void TearDown() override {
    coordinator_.reset();
  }

  void RegisterClientProcess(
      mojo::PendingReceiver<mojom::Coordinator> receiver,
      mojo::PendingRemote<mojom::ClientProcess> client_process,
      mojom::ProcessType process_type,
      base::ProcessId pid) {
    coordinator_->RegisterClientProcess(
        std::move(receiver), std::move(client_process), process_type, pid,
        /*service_name=*/std::nullopt);
  }

  void RequestGlobalMemoryDump(RequestGlobalMemoryDumpCallback callback) {
    RequestGlobalMemoryDump(
        MemoryDumpType::kSummaryOnly, MemoryDumpLevelOfDetail::kBackground,
        MemoryDumpDeterminism::kNone, {}, std::move(callback));
  }

  void RequestGlobalMemoryDump(
      MemoryDumpType dump_type,
      MemoryDumpLevelOfDetail level_of_detail,
      MemoryDumpDeterminism determinism,
      const std::vector<std::string>& allocator_dump_names,
      RequestGlobalMemoryDumpCallback callback) {
    coordinator_->RequestGlobalMemoryDump(dump_type, level_of_detail,
                                          determinism, allocator_dump_names,
                                          std::move(callback));
  }

  void RequestGlobalMemoryDumpForPid(
      base::ProcessId pid,
      const std::vector<std::string>& allocator_dump_names,
      RequestGlobalMemoryDumpForPidCallback callback) {
    coordinator_->RequestGlobalMemoryDumpForPid(pid, allocator_dump_names,
                                                std::move(callback));
  }

  void RequestGlobalMemoryDumpAndAppendToTrace(
      RequestGlobalMemoryDumpAndAppendToTraceCallback callback) {
    coordinator_->RequestGlobalMemoryDumpAndAppendToTrace(
        MemoryDumpType::kExplicitlyTriggered,
        MemoryDumpLevelOfDetail::kDetailed, MemoryDumpDeterminism::kNone,
        std::move(callback));
  }

  void GetVmRegionsForHeapProfiler(
      const std::vector<base::ProcessId>& pids,
      GetVmRegionsForHeapProfilerCallback callback) {
    coordinator_->GetVmRegionsForHeapProfiler(pids, std::move(callback));
  }

  void ReduceCoordinatorClientProcessTimeout() {
    coordinator_->set_client_process_timeout(base::Milliseconds(5));
  }

 protected:
  // Note that |coordinator_| must outlive |task_environment_|, because
  // otherwise a worker thread owned by the task environment may try to access
  // the coordinator after it has been destroyed.
  std::unique_ptr<NiceMock<FakeCoordinatorImpl>> coordinator_;
  base::test::TaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  base::test::TracingEnvironment tracing_environment_;
};

class MockClientProcess : public mojom::ClientProcess {
 public:
  MockClientProcess(CoordinatorImplTest* test_coordinator)
      : MockClientProcess(test_coordinator,
                          base::GetCurrentProcId(),
                          mojom::ProcessType::OTHER) {}

  MockClientProcess(CoordinatorImplTest* test_coordinator,
                    base::ProcessId pid,
                    mojom::ProcessType process_type) {
    // Register to the coordinator.
    mojo::Remote<mojom::Coordinator> remote_coordinator;
    mojo::PendingRemote<mojom::ClientProcess> client_process;
    receiver_.Bind(client_process.InitWithNewPipeAndPassReceiver());
    test_coordinator->RegisterClientProcess(
        remote_coordinator.BindNewPipeAndPassReceiver(),
        std::move(client_process), process_type, pid);

    ON_CALL(*this, RequestChromeMemoryDumpMock(_, _))
        .WillByDefault(Invoke([pid](const MemoryDumpRequestArgs& args,
                                    RequestChromeMemoryDumpCallback& callback) {
          MemoryDumpArgs dump_args{MemoryDumpLevelOfDetail::kDetailed};
          auto pmd = std::make_unique<ProcessMemoryDump>(dump_args);
          auto* mad = pmd->CreateAllocatorDump(
              "malloc", base::trace_event::MemoryAllocatorDumpGuid(pid));
          mad->AddScalar(MemoryAllocatorDump::kNameSize,
                         MemoryAllocatorDump::kUnitsBytes, 1024);

          std::move(callback).Run(true, args.dump_guid, std::move(pmd));
        }));

    ON_CALL(*this, RequestOSMemoryDumpMock(_, _, _))
        .WillByDefault(Invoke([](mojom::MemoryMapOption,
                                 const std::vector<base::ProcessId> pids,
                                 RequestOSMemoryDumpCallback& callback) {
          base::flat_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
          std::move(callback).Run(true, std::move(results));
        }));
  }

  ~MockClientProcess() override = default;

  // TODO(crbug.com/40524294): Remove non const reference here once GMock is
  // updated to support move-only types.
  MOCK_METHOD2(RequestChromeMemoryDumpMock,
               void(const MemoryDumpRequestArgs& args,
                    RequestChromeMemoryDumpCallback& callback));
  MOCK_METHOD3(RequestOSMemoryDumpMock,
               void(mojom::MemoryMapOption option,
                    const std::vector<base::ProcessId>& args,
                    RequestOSMemoryDumpCallback& callback));

  void RequestChromeMemoryDump(
      const MemoryDumpRequestArgs& args,
      RequestChromeMemoryDumpCallback callback) override {
    RequestChromeMemoryDumpMock(args, callback);
  }
  void RequestOSMemoryDump(mojom::MemoryMapOption option,
                           const std::vector<base::ProcessId>& args,
                           RequestOSMemoryDumpCallback callback) override {
    RequestOSMemoryDumpMock(option, args, callback);
  }

 private:
  mojo::Receiver<mojom::ClientProcess> receiver_{this};
};

class MockGlobalMemoryDumpCallback {
 public:
  MockGlobalMemoryDumpCallback() = default;
  MOCK_METHOD2(OnCall, void(bool, GlobalMemoryDump*));

  void Run(bool success, GlobalMemoryDumpPtr ptr) {
    OnCall(success, ptr.get());
  }

  RequestGlobalMemoryDumpCallback Get() {
    return base::BindOnce(&MockGlobalMemoryDumpCallback::Run,
                          base::Unretained(this));
  }
};

class MockGlobalMemoryDumpAndAppendToTraceCallback {
 public:
  MockGlobalMemoryDumpAndAppendToTraceCallback() = default;
  MOCK_METHOD2(OnCall, void(bool, uint64_t));

  void Run(bool success, uint64_t dump_guid) { OnCall(success, dump_guid); }

  RequestGlobalMemoryDumpAndAppendToTraceCallback Get() {
    return base::BindOnce(&MockGlobalMemoryDumpAndAppendToTraceCallback::Run,
                          base::Unretained(this));
  }
};

class MockGetVmRegionsForHeapProfilerCallback {
 public:
  MockGetVmRegionsForHeapProfilerCallback() = default;
  MOCK_METHOD1(OnCall,
               void(const base::flat_map<base::ProcessId,
                                         std::vector<mojom::VmRegionPtr>>&));

  void Run(base::flat_map<base::ProcessId, std::vector<mojom::VmRegionPtr>>
               results) {
    OnCall(results);
  }

  GetVmRegionsForHeapProfilerCallback Get() {
    return base::BindOnce(&MockGetVmRegionsForHeapProfilerCallback::Run,
                          base::Unretained(this));
  }
};

uint64_t GetFakeAddrForVmRegion(int pid, int region_index) {
  return 0x100000ul * pid * (region_index + 1);
}

uint64_t GetFakeSizeForVmRegion(int pid, int region_index) {
  return 4096 * pid * (region_index + 1);
}

mojom::RawOSMemDumpPtr FillRawOSDump(int pid) {
  mojom::RawOSMemDumpPtr raw_os_dump = mojom::RawOSMemDump::New();
  raw_os_dump->platform_private_footprint =
      mojom::PlatformPrivateFootprint::New();
  raw_os_dump->resident_set_kb = pid;
  for (int i = 0; i < 3; i++) {
    mojom::VmRegionPtr vm_region = mojom::VmRegion::New();
    vm_region->start_address = GetFakeAddrForVmRegion(pid, i);
    vm_region->size_in_bytes = GetFakeSizeForVmRegion(pid, i);
    raw_os_dump->memory_maps.push_back(std::move(vm_region));
  }
  return raw_os_dump;
}

// Tests that the global dump is acked even in absence of clients.
TEST_F(CoordinatorImplTest, NoClients) {
  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(callback, OnCall(true, NotNull()));
  RequestGlobalMemoryDump(callback.Get());
}

// Nominal behavior: several clients contributing to the global dump.
TEST_F(CoordinatorImplTest, SeveralClients) {
  base::RunLoop run_loop;

  NiceMock<MockClientProcess> client_process_1(this, 1,
                                               mojom::ProcessType::BROWSER);
  NiceMock<MockClientProcess> client_process_2(this);

  EXPECT_CALL(client_process_1, RequestChromeMemoryDumpMock(_, _)).Times(1);
  EXPECT_CALL(client_process_2, RequestChromeMemoryDumpMock(_, _)).Times(1);

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(callback, OnCall(true, NotNull()))
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  RequestGlobalMemoryDump(callback.Get());
  run_loop.Run();
}

// Issuing two requests will cause the second one to be queued.
TEST_F(CoordinatorImplTest, QueuedRequest) {
  base::RunLoop run_loop;

  // This variable to be static as the lambda below has to convert to a function
  // pointer rather than a functor.
  static base::test::TaskEnvironment* task_environment = nullptr;
  task_environment = &task_environment_;

  NiceMock<MockClientProcess> client_process_1(this, 1,
                                               mojom::ProcessType::BROWSER);
  NiceMock<MockClientProcess> client_process_2(this);

  // Each request will invoke on both processes.
  EXPECT_CALL(client_process_1, RequestChromeMemoryDumpMock(_, _)).Times(2);
  EXPECT_CALL(client_process_2, RequestChromeMemoryDumpMock(_, _))
      .Times(2)
      .WillRepeatedly(Invoke(
          [](const MemoryDumpRequestArgs& args,
             MockClientProcess::RequestChromeMemoryDumpCallback& callback) {
            // Skip the wall clock time-ticks forward to make sure start_time
            // is strictly increasing.
            task_environment->FastForwardBy(base::Milliseconds(10));
            MemoryDumpArgs dump_args{MemoryDumpLevelOfDetail::kDetailed};
            auto pmd = std::make_unique<ProcessMemoryDump>(dump_args);
            std::move(callback).Run(true, args.dump_guid, std::move(pmd));
          }));

  MockGlobalMemoryDumpCallback callback1;
  MockGlobalMemoryDumpCallback callback2;

  // Verify that the start time of subsequent dumps is monotonically
  // increasing.
  base::TimeTicks before = base::TimeTicks::Now();
  base::TimeTicks first_dump_time;
  EXPECT_CALL(callback1, OnCall(true, NotNull()))
      .WillOnce(Invoke([&](bool success, GlobalMemoryDump* global_dump) {
        EXPECT_LE(before, global_dump->start_time);
        first_dump_time = global_dump->start_time;
      }));
  EXPECT_CALL(callback2, OnCall(true, NotNull()))
      .WillOnce(Invoke([&](bool success, GlobalMemoryDump* global_dump) {
        EXPECT_LT(before, global_dump->start_time);
        EXPECT_LT(first_dump_time, global_dump->start_time);
        run_loop.Quit();
      }));
  RequestGlobalMemoryDump(callback1.Get());
  RequestGlobalMemoryDump(callback2.Get());
  run_loop.Run();
}

TEST_F(CoordinatorImplTest, MissingChromeDump) {
  base::RunLoop run_loop;

  NiceMock<MockClientProcess> client_process(this, 1,
                                             mojom::ProcessType::BROWSER);

  EXPECT_CALL(client_process, RequestChromeMemoryDumpMock(_, _))
      .WillOnce(Invoke(
          [](const MemoryDumpRequestArgs& args,
             MockClientProcess::RequestChromeMemoryDumpCallback& callback) {
            MemoryDumpArgs dump_args{MemoryDumpLevelOfDetail::kDetailed};
            auto pmd = std::make_unique<ProcessMemoryDump>(dump_args);
            std::move(callback).Run(true, args.dump_guid, std::move(pmd));
          }));

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(
      callback,
      OnCall(true, Pointee(Field(&mojom::GlobalMemoryDump::process_dumps,
                                 IsEmpty()))))
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  RequestGlobalMemoryDump(callback.Get());
  run_loop.Run();
}

TEST_F(CoordinatorImplTest, MissingOsDump) {
  base::RunLoop run_loop;

  NiceMock<MockClientProcess> client_process(this, 1,
                                             mojom::ProcessType::BROWSER);

  EXPECT_CALL(client_process, RequestOSMemoryDumpMock(_, _, _))
      .WillOnce(Invoke(
          [](mojom::MemoryMapOption, const std::vector<base::ProcessId>& pids,
             MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            base::flat_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
            std::move(callback).Run(true, std::move(results));
          }));

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(
      callback,
      OnCall(true, Pointee(Field(&mojom::GlobalMemoryDump::process_dumps,
                                 IsEmpty()))))
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  RequestGlobalMemoryDump(callback.Get());
  run_loop.Run();
}

TEST_F(CoordinatorImplTest, TimeOutStuckChild) {
  base::RunLoop run_loop;

  // |stuck_callback| should be destroyed after |client_process| or mojo
  // will complain about the callback being destoyed before the binding.
  MockClientProcess::RequestChromeMemoryDumpCallback stuck_callback;
  NiceMock<MockClientProcess> client_process(this, 1,
                                             mojom::ProcessType::BROWSER);

  // Store a reference to the callback passed to RequestChromeMemoryDump
  // to emulate "stuck" behaviour.
  EXPECT_CALL(client_process, RequestChromeMemoryDumpMock(_, _))
      .WillOnce(Invoke(
          [&stuck_callback](
              const MemoryDumpRequestArgs&,
              MockClientProcess::RequestChromeMemoryDumpCallback& callback) {
            stuck_callback = std::move(callback);
          }));

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(
      callback,
      OnCall(false, Pointee(Field(&mojom::GlobalMemoryDump::process_dumps,
                                  IsEmpty()))))
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  ReduceCoordinatorClientProcessTimeout();
  RequestGlobalMemoryDump(callback.Get());
  run_loop.Run();
}

TEST_F(CoordinatorImplTest, TimeOutStuckChildMultiProcess) {
  base::RunLoop run_loop;

  static constexpr base::ProcessId kBrowserPid = 1;
  static constexpr base::ProcessId kRendererPid = 2;

  // |stuck_callback| should be destroyed after |renderer_client| or mojo
  // will complain about the callback being destoyed before the binding.
  MockClientProcess::RequestChromeMemoryDumpCallback stuck_callback;
  MockClientProcess browser_client(this, kBrowserPid,
                                   mojom::ProcessType::BROWSER);
  MockClientProcess renderer_client(this, kRendererPid,
                                    mojom::ProcessType::RENDERER);

// This ifdef is here to match the sandboxing behavior of the client.
// On Linux, all memory dumps come from the browser client. On all other
// platforms, they are expected to come from each individual client.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_CALL(browser_client,
              RequestOSMemoryDumpMock(
                  _, AllOf(Contains(kBrowserPid), Contains(kRendererPid)), _))
      .WillOnce(Invoke(
          [](mojom::MemoryMapOption, const std::vector<base::ProcessId>& pids,
             MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            base::flat_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
            results[kBrowserPid] = FillRawOSDump(kBrowserPid);
            results[kRendererPid] = FillRawOSDump(kRendererPid);
            std::move(callback).Run(true, std::move(results));
          }));
  EXPECT_CALL(renderer_client, RequestOSMemoryDumpMock(_, _, _)).Times(0);
#else
  EXPECT_CALL(browser_client, RequestOSMemoryDumpMock(_, Contains(0), _))
      .WillOnce(Invoke(
          [](mojom::MemoryMapOption, const std::vector<base::ProcessId>& pids,
             MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            base::flat_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
            results[0] = FillRawOSDump(kBrowserPid);
            std::move(callback).Run(true, std::move(results));
          }));
  EXPECT_CALL(renderer_client, RequestOSMemoryDumpMock(_, Contains(0), _))
      .WillOnce(Invoke(
          [](mojom::MemoryMapOption, const std::vector<base::ProcessId>& pids,
             MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            base::flat_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
            results[0] = FillRawOSDump(kRendererPid);
            std::move(callback).Run(true, std::move(results));
          }));
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  // Make the browser respond correctly but pretend the renderer is "stuck"
  // by storing a callback.
  EXPECT_CALL(renderer_client, RequestChromeMemoryDumpMock(_, _))
      .WillOnce(Invoke(
          [&stuck_callback](
              const MemoryDumpRequestArgs&,
              MockClientProcess::RequestChromeMemoryDumpCallback& callback) {
            stuck_callback = std::move(callback);
          }));

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(callback, OnCall(false, _))
      .WillOnce(
          Invoke([&run_loop](bool success, GlobalMemoryDump* global_dump) {
            EXPECT_EQ(1U, global_dump->process_dumps.size());
            run_loop.Quit();
          }));
  ReduceCoordinatorClientProcessTimeout();
  RequestGlobalMemoryDump(callback.Get());
  run_loop.Run();
}

// Tests that a global dump is completed even if a client disconnects (e.g. due
// to a crash) while a global dump is happening.
TEST_F(CoordinatorImplTest, ClientCrashDuringGlobalDump) {
  base::RunLoop run_loop;

  auto client_process_1 = std::make_unique<NiceMock<MockClientProcess>>(
      this, 1, mojom::ProcessType::BROWSER);
  auto client_process_2 = std::make_unique<NiceMock<MockClientProcess>>(this);

  // One of the client processes dies after a global dump is requested and
  // before it receives the corresponding process dump request. The coordinator
  // should detect that one of its clients is disconnected and claim the global
  // dump attempt has failed.

  // Whichever client is called first destroys the other client.
  ON_CALL(*client_process_1, RequestChromeMemoryDumpMock(_, _))
      .WillByDefault(Invoke(
          [&client_process_2](
              const MemoryDumpRequestArgs& args,
              MockClientProcess::RequestChromeMemoryDumpCallback& callback) {
            client_process_2.reset();
            std::move(callback).Run(true, args.dump_guid, nullptr);
          }));
  ON_CALL(*client_process_2, RequestChromeMemoryDumpMock(_, _))
      .WillByDefault(Invoke(
          [&client_process_1](
              const MemoryDumpRequestArgs& args,
              MockClientProcess::RequestChromeMemoryDumpCallback& callback) {
            client_process_1.reset();
            std::move(callback).Run(true, args.dump_guid, nullptr);
          }));

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(callback, OnCall(false, NotNull()))
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  RequestGlobalMemoryDump(callback.Get());
  run_loop.Run();
}

// Like ClientCrashDuringGlobalDump but covers the case of having only one
// client. Regression testing for crbug.com/742265.
TEST_F(CoordinatorImplTest, SingleClientCrashDuringGlobalDump) {
  base::RunLoop run_loop;

  auto client_process = std::make_unique<NiceMock<MockClientProcess>>(
      this, 1, mojom::ProcessType::BROWSER);

  ON_CALL(*client_process, RequestChromeMemoryDumpMock(_, _))
      .WillByDefault(Invoke(
          [&client_process](
              const MemoryDumpRequestArgs& args,
              MockClientProcess::RequestChromeMemoryDumpCallback& callback) {
            // The dtor here will cause mojo to post an UnregisterClient call to
            // the coordinator.
            client_process.reset();
            std::move(callback).Run(true, args.dump_guid, nullptr);
          }));

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(callback, OnCall(false, NotNull()))
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  RequestGlobalMemoryDump(callback.Get());
  run_loop.Run();
}

TEST_F(CoordinatorImplTest, GlobalMemoryDumpStruct) {
  base::RunLoop run_loop;

  MockClientProcess browser_client(this, 1, mojom::ProcessType::BROWSER);
  MockClientProcess renderer_client(this, 2, mojom::ProcessType::RENDERER);

  EXPECT_CALL(browser_client, RequestChromeMemoryDumpMock(_, _))
      .WillOnce(Invoke([](const MemoryDumpRequestArgs& args,
                          MockClientProcess::RequestChromeMemoryDumpCallback&
                              callback) {
        MemoryDumpArgs dump_args{MemoryDumpLevelOfDetail::kDetailed};
        auto pmd = std::make_unique<ProcessMemoryDump>(dump_args);
        auto* size = MemoryAllocatorDump::kNameSize;
        auto* bytes = MemoryAllocatorDump::kUnitsBytes;
        const uint32_t kB = 1024;

        pmd->CreateAllocatorDump("malloc",
                                 base::trace_event::MemoryAllocatorDumpGuid(1))
            ->AddScalar(size, bytes, 1 * kB);
        pmd->CreateAllocatorDump("malloc/ignored")
            ->AddScalar(size, bytes, 99 * kB);

        pmd->CreateAllocatorDump("blink_gc")->AddScalar(size, bytes, 2 * kB);
        pmd->CreateAllocatorDump("blink_gc/ignored")
            ->AddScalar(size, bytes, 99 * kB);

        pmd->CreateAllocatorDump("v8/foo")->AddScalar(size, bytes, 1 * kB);
        pmd->CreateAllocatorDump("v8/bar")->AddScalar(size, bytes, 2 * kB);
        pmd->CreateAllocatorDump("v8")->AddScalar(size, bytes, 99 * kB);

        // All the 99 KB values here are expected to be ignored.
        pmd->CreateAllocatorDump("partition_alloc")
            ->AddScalar(size, bytes, 99 * kB);
        pmd->CreateAllocatorDump("partition_alloc/allocated_objects")
            ->AddScalar(size, bytes, 99 * kB);
        pmd->CreateAllocatorDump("partition_alloc/allocated_objects/ignored")
            ->AddScalar(size, bytes, 99 * kB);
        pmd->CreateAllocatorDump("partition_alloc/partitions")
            ->AddScalar(size, bytes, 99 * kB);
        pmd->CreateAllocatorDump("partition_alloc/partitions/not_ignored_1")
            ->AddScalar(size, bytes, 2 * kB);
        pmd->CreateAllocatorDump("partition_alloc/partitions/not_ignored_2")
            ->AddScalar(size, bytes, 2 * kB);

        std::move(callback).Run(true, args.dump_guid, std::move(pmd));
      }));
  EXPECT_CALL(renderer_client, RequestChromeMemoryDumpMock(_, _))
      .WillOnce(Invoke(
          [](const MemoryDumpRequestArgs& args,
             MockClientProcess::RequestChromeMemoryDumpCallback& callback) {
            MemoryDumpArgs dump_args{MemoryDumpLevelOfDetail::kDetailed};
            auto pmd = std::make_unique<ProcessMemoryDump>(dump_args);
            auto* mad = pmd->CreateAllocatorDump(
                "malloc", base::trace_event::MemoryAllocatorDumpGuid(2));
            mad->AddScalar(MemoryAllocatorDump::kNameSize,
                           MemoryAllocatorDump::kUnitsBytes, 1024 * 2);
            std::move(callback).Run(true, args.dump_guid, std::move(pmd));
          }));
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_CALL(browser_client,
              RequestOSMemoryDumpMock(_, AllOf(Contains(1), Contains(2)), _))
      .WillOnce(Invoke(
          [](mojom::MemoryMapOption, const std::vector<base::ProcessId>& pids,
             MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            base::flat_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
            results[1] = mojom::RawOSMemDump::New();
            results[1]->resident_set_kb = 1;
            results[1]->platform_private_footprint =
                mojom::PlatformPrivateFootprint::New();
            results[2] = mojom::RawOSMemDump::New();
            results[2]->platform_private_footprint =
                mojom::PlatformPrivateFootprint::New();
            results[2]->resident_set_kb = 2;
            std::move(callback).Run(true, std::move(results));
          }));
  EXPECT_CALL(renderer_client, RequestOSMemoryDumpMock(_, _, _)).Times(0);
#else
  EXPECT_CALL(browser_client, RequestOSMemoryDumpMock(_, Contains(0), _))
      .WillOnce(Invoke(
          [](mojom::MemoryMapOption, const std::vector<base::ProcessId>& pids,
             MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            base::flat_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
            results[0] = mojom::RawOSMemDump::New();
            results[0]->platform_private_footprint =
                mojom::PlatformPrivateFootprint::New();
            results[0]->resident_set_kb = 1;
            std::move(callback).Run(true, std::move(results));
          }));
  EXPECT_CALL(renderer_client, RequestOSMemoryDumpMock(_, Contains(0), _))
      .WillOnce(Invoke(
          [](mojom::MemoryMapOption, const std::vector<base::ProcessId>& pids,
             MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            base::flat_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
            results[0] = mojom::RawOSMemDump::New();
            results[0]->platform_private_footprint =
                mojom::PlatformPrivateFootprint::New();
            results[0]->resident_set_kb = 2;
            std::move(callback).Run(true, std::move(results));
          }));
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(callback, OnCall(true, NotNull()))
      .WillOnce(Invoke([&run_loop](bool success,
                                   GlobalMemoryDump* global_dump) {
        EXPECT_TRUE(success);
        EXPECT_EQ(2U, global_dump->process_dumps.size());
        mojom::ProcessMemoryDumpPtr browser_dump = nullptr;
        mojom::ProcessMemoryDumpPtr renderer_dump = nullptr;
        for (mojom::ProcessMemoryDumpPtr& dump : global_dump->process_dumps) {
          if (dump->process_type == mojom::ProcessType::BROWSER) {
            browser_dump = std::move(dump);
          } else if (dump->process_type == mojom::ProcessType::RENDERER) {
            renderer_dump = std::move(dump);
          }
        }

        EXPECT_EQ(browser_dump->os_dump->resident_set_kb, 1u);
        EXPECT_EQ(renderer_dump->os_dump->resident_set_kb, 2u);
        run_loop.Quit();
      }));

  RequestGlobalMemoryDump(callback.Get());
  run_loop.Run();
}

TEST_F(CoordinatorImplTest, VmRegionsForHeapProfiler) {
  base::RunLoop run_loop;
  // Not using a constexpr base::ProcessId because std:unordered_map<>
  // and friends makes it too easy to accidentally odr-use this variable
  // causing all sorts of compiler-toolchain divergent fun when trying
  // to decide of the lambda capture is necessary.
  static constexpr base::ProcessId kBrowserPid = 1;
  static constexpr base::ProcessId kRendererPid = 2;

  MockClientProcess browser_client(this, kBrowserPid,
                                   mojom::ProcessType::BROWSER);
  MockClientProcess renderer_client(this, kRendererPid,
                                    mojom::ProcessType::RENDERER);

// This ifdef is here to match the sandboxing behavior of the client.
// On Linux, all memory dumps come from the browser client. On all other
// platforms, they are expected to come from each individual client.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_CALL(browser_client,
              RequestOSMemoryDumpMock(
                  _, AllOf(Contains(kBrowserPid), Contains(kRendererPid)), _))
      .WillOnce(Invoke(
          [](mojom::MemoryMapOption, const std::vector<base::ProcessId>& pids,
             MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            base::flat_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
            results[kBrowserPid] = FillRawOSDump(kBrowserPid);
            results[kRendererPid] = FillRawOSDump(kRendererPid);
            std::move(callback).Run(true, std::move(results));
          }));
  EXPECT_CALL(renderer_client, RequestOSMemoryDumpMock(_, _, _)).Times(0);
#else
  EXPECT_CALL(browser_client, RequestOSMemoryDumpMock(_, Contains(0), _))
      .WillOnce(Invoke(
          [](mojom::MemoryMapOption, const std::vector<base::ProcessId>& pids,
             MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            base::flat_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
            results[0] = FillRawOSDump(kBrowserPid);
            std::move(callback).Run(true, std::move(results));
          }));
  EXPECT_CALL(renderer_client, RequestOSMemoryDumpMock(_, Contains(0), _))
      .WillOnce(Invoke(
          [](mojom::MemoryMapOption, const std::vector<base::ProcessId>& pids,
             MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            base::flat_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
            results[0] = FillRawOSDump(kRendererPid);
            std::move(callback).Run(true, std::move(results));
          }));
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  MockGetVmRegionsForHeapProfilerCallback callback;
  EXPECT_CALL(callback, OnCall(_))
      .WillOnce(Invoke(
          [&run_loop](
              const base::flat_map<base::ProcessId,
                                   std::vector<mojom::VmRegionPtr>>& results) {
            ASSERT_EQ(2U, results.size());

            auto browser_it = results.find(kBrowserPid);
            ASSERT_TRUE(browser_it != results.end());
            auto renderer_it = results.find(kRendererPid);
            ASSERT_TRUE(renderer_it != results.end());

            const std::vector<mojom::VmRegionPtr>& browser_mmaps =
                browser_it->second;
            ASSERT_EQ(3u, browser_mmaps.size());
            for (int i = 0; i < 3; i++) {
              EXPECT_EQ(GetFakeAddrForVmRegion(kBrowserPid, i),
                        browser_mmaps[i]->start_address);
            }

            const std::vector<mojom::VmRegionPtr>& renderer_mmaps =
                renderer_it->second;
            ASSERT_EQ(3u, renderer_mmaps.size());
            for (int i = 0; i < 3; i++) {
              EXPECT_EQ(GetFakeAddrForVmRegion(kRendererPid, i),
                        renderer_mmaps[i]->start_address);
            }
            run_loop.Quit();
          }));

  std::vector<base::ProcessId> pids;
  pids.push_back(kBrowserPid);
  pids.push_back(kRendererPid);
  GetVmRegionsForHeapProfiler(pids, callback.Get());
  run_loop.Run();
}

// RequestGlobalMemoryDump, as opposite to RequestGlobalMemoryDumpAndAddToTrace,
// shouldn't add anything into the trace
TEST_F(CoordinatorImplTest, DumpsArentAddedToTraceUnlessRequested) {
  tracing::DataSourceTester data_source_tester(
      TracingObserverProto::GetInstance());

  base::RunLoop run_loop;

  NiceMock<MockClientProcess> client_process(this, 1,
                                             mojom::ProcessType::BROWSER);

  EXPECT_CALL(client_process, RequestChromeMemoryDumpMock(_, _))
      .WillOnce(Invoke(
          [](const MemoryDumpRequestArgs& args,
             MockClientProcess::RequestChromeMemoryDumpCallback& callback) {
            MemoryDumpArgs dump_args{MemoryDumpLevelOfDetail::kDetailed};
            auto pmd = std::make_unique<ProcessMemoryDump>(dump_args);
            std::move(callback).Run(true, args.dump_guid, std::move(pmd));
          }));

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(
      callback,
      OnCall(true, Pointee(Field(&mojom::GlobalMemoryDump::process_dumps,
                                 IsEmpty()))))
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

  base::trace_event::TraceConfig trace_config(
      std::string(MemoryDumpManager::kTraceCategory) + ",-*",
      base::trace_event::RECORD_UNTIL_FULL);
  data_source_tester.BeginTrace(trace_config);
  RequestGlobalMemoryDump(MemoryDumpType::kExplicitlyTriggered,
                          MemoryDumpLevelOfDetail::kDetailed,
                          MemoryDumpDeterminism::kNone, {}, callback.Get());
  run_loop.Run();
  data_source_tester.EndTracing();

  for (size_t i = 0; i < data_source_tester.GetFinalizedPacketCount(); i++) {
    EXPECT_FALSE(data_source_tester.GetFinalizedPacket(i)
                     ->has_memory_tracker_snapshot());
  }
}

// TODO(crbug.com/40281135): Test is flaky across platforms.
TEST_F(CoordinatorImplTest, DISABLED_DumpsAreAddedToTraceWhenRequested) {
  tracing::DataSourceTester data_source_tester(
      TracingObserverProto::GetInstance());

  base::RunLoop run_loop;

  NiceMock<MockClientProcess> client_process(this, 1,
                                             mojom::ProcessType::BROWSER);
  EXPECT_CALL(client_process, RequestChromeMemoryDumpMock(_, _))
      .WillOnce(Invoke(
          [](const MemoryDumpRequestArgs& args,
             MockClientProcess::RequestChromeMemoryDumpCallback& callback) {
            MemoryDumpArgs dump_args{MemoryDumpLevelOfDetail::kDetailed};
            auto pmd = std::make_unique<ProcessMemoryDump>(dump_args);
            std::move(callback).Run(true, args.dump_guid, std::move(pmd));
          }));

  MockGlobalMemoryDumpAndAppendToTraceCallback callback;
  EXPECT_CALL(callback, OnCall(true, Ne(0ul)))
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

  base::trace_event::TraceConfig trace_config(
      std::string(MemoryDumpManager::kTraceCategory) + ",-*",
      base::trace_event::RECORD_UNTIL_FULL);
  data_source_tester.BeginTrace(trace_config);
  RequestGlobalMemoryDumpAndAppendToTrace(callback.Get());
  run_loop.Run();
  data_source_tester.EndTracing();

  EXPECT_GE(data_source_tester.GetFinalizedPacketCount(), 1u);
  const auto* packet = data_source_tester.GetFinalizedPacket();
  EXPECT_EQ(packet->memory_tracker_snapshot().level_of_detail(),
            perfetto::protos::MemoryTrackerSnapshot::DETAIL_FULL);
}

TEST_F(CoordinatorImplTest, DumpByPidSuccess) {
  static constexpr base::ProcessId kBrowserPid = 1;
  static constexpr base::ProcessId kRendererPid = 2;
  static constexpr base::ProcessId kGpuPid = 3;

  NiceMock<MockClientProcess> client_process_1(this, kBrowserPid,
                                               mojom::ProcessType::BROWSER);
  NiceMock<MockClientProcess> client_process_2(this, kRendererPid,
                                               mojom::ProcessType::RENDERER);
  NiceMock<MockClientProcess> client_process_3(this, kGpuPid,
                                               mojom::ProcessType::GPU);

// This ifdef is here to match the sandboxing behavior of the client.
// On Linux, all memory dumps come from the browser client. On all other
// platforms, they are expected to come from each individual client.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_CALL(client_process_1, RequestOSMemoryDumpMock(_, _, _))
      .WillOnce(Invoke(
          [](mojom::MemoryMapOption, const std::vector<base::ProcessId>& pids,
             MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            base::flat_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
            results[kBrowserPid] = FillRawOSDump(kBrowserPid);
            std::move(callback).Run(true, std::move(results));
          }))
      .WillOnce(Invoke(
          [](mojom::MemoryMapOption, const std::vector<base::ProcessId>& pids,
             MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            base::flat_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
            results[kRendererPid] = FillRawOSDump(kRendererPid);
            std::move(callback).Run(true, std::move(results));
          }))
      .WillOnce(Invoke(
          [](mojom::MemoryMapOption, const std::vector<base::ProcessId>& pids,
             MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            base::flat_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
            results[kGpuPid] = FillRawOSDump(kGpuPid);
            std::move(callback).Run(true, std::move(results));
          }));
#else
  EXPECT_CALL(client_process_1, RequestOSMemoryDumpMock(_, Contains(0), _))
      .WillOnce(Invoke(
          [](mojom::MemoryMapOption, const std::vector<base::ProcessId>& pids,
             MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            base::flat_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
            results[0] = FillRawOSDump(kBrowserPid);
            std::move(callback).Run(true, std::move(results));
          }));
  EXPECT_CALL(client_process_2, RequestOSMemoryDumpMock(_, Contains(0), _))
      .WillOnce(Invoke(
          [](mojom::MemoryMapOption, const std::vector<base::ProcessId>& pids,
             MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            base::flat_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
            results[0] = FillRawOSDump(kRendererPid);
            std::move(callback).Run(true, std::move(results));
          }));
  EXPECT_CALL(client_process_3, RequestOSMemoryDumpMock(_, Contains(0), _))
      .WillOnce(Invoke(
          [](mojom::MemoryMapOption, const std::vector<base::ProcessId>& pids,
             MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            base::flat_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
            results[0] = FillRawOSDump(kGpuPid);
            std::move(callback).Run(true, std::move(results));
          }));
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  base::RunLoop run_loop;

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(callback, OnCall(true, Ne(nullptr)))
      .WillOnce(Invoke([](bool success, GlobalMemoryDump* global_dump) {
        EXPECT_EQ(1U, global_dump->process_dumps.size());
        EXPECT_EQ(global_dump->process_dumps[0]->pid, kBrowserPid);
      }))
      .WillOnce(Invoke([](bool success, GlobalMemoryDump* global_dump) {
        EXPECT_EQ(1U, global_dump->process_dumps.size());
        EXPECT_EQ(global_dump->process_dumps[0]->pid, kRendererPid);
      }))
      .WillOnce(
          Invoke([&run_loop](bool success, GlobalMemoryDump* global_dump) {
            EXPECT_EQ(1U, global_dump->process_dumps.size());
            EXPECT_EQ(global_dump->process_dumps[0]->pid, kGpuPid);
            run_loop.Quit();
          }));

  RequestGlobalMemoryDumpForPid(kBrowserPid, {}, callback.Get());
  RequestGlobalMemoryDumpForPid(kRendererPid, {}, callback.Get());
  RequestGlobalMemoryDumpForPid(kGpuPid, {}, callback.Get());
  run_loop.Run();
}

TEST_F(CoordinatorImplTest, DumpByPidFailure) {
  NiceMock<MockClientProcess> client_process_1(this, 1,
                                               mojom::ProcessType::BROWSER);

  base::RunLoop run_loop;

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(callback, OnCall(false, nullptr))
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

  RequestGlobalMemoryDumpForPid(2, {}, callback.Get());
  run_loop.Run();
}

TEST_F(CoordinatorImplTest, GlobalDumpWithSubTrees) {
  base::RunLoop run_loop;

  static constexpr base::ProcessId kBrowserPid = 1;
  MockClientProcess browser_client(this, kBrowserPid,
                                   mojom::ProcessType::BROWSER);

  EXPECT_CALL(browser_client, RequestChromeMemoryDumpMock(_, _))
      .WillOnce(Invoke(
          [](const MemoryDumpRequestArgs& args,
             MockClientProcess::RequestChromeMemoryDumpCallback& callback) {
            MemoryDumpArgs dump_args{MemoryDumpLevelOfDetail::kDetailed};
            auto pmd = std::make_unique<ProcessMemoryDump>(dump_args);
            auto* size = MemoryAllocatorDump::kNameSize;
            auto* bytes = MemoryAllocatorDump::kUnitsBytes;
            const uint32_t kB = 1024;

            // None of these dumps should appear in the output.
            pmd->CreateAllocatorDump(
                   "malloc", base::trace_event::MemoryAllocatorDumpGuid(1))
                ->AddScalar(size, bytes, 1 * kB);
            pmd->CreateAllocatorDump("v8/foo")->AddScalar(size, bytes, 1 * kB);
            pmd->CreateAllocatorDump("v8/bar")->AddScalar(size, bytes, 2 * kB);
            pmd->CreateAllocatorDump("v8")->AddScalar(size, bytes, 99 * kB);

            // This "tree" of dumps should be included.
            pmd->CreateAllocatorDump("partition_alloc")
                ->AddScalar(size, bytes, 99 * kB);
            pmd->CreateAllocatorDump("partition_alloc/allocated_objects")
                ->AddScalar(size, bytes, 99 * kB);
            pmd->CreateAllocatorDump("partition_alloc/partitions")
                ->AddScalar(size, bytes, 99 * kB);
            pmd->CreateAllocatorDump("partition_alloc/partitions/1")
                ->AddScalar(size, bytes, 2 * kB);
            pmd->CreateAllocatorDump("partition_alloc/partitions/2")
                ->AddScalar(size, bytes, 2 * kB);

            std::move(callback).Run(true, args.dump_guid, std::move(pmd));
          }));
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_CALL(browser_client, RequestOSMemoryDumpMock(_, Contains(1), _))
      .WillOnce(Invoke(
          [](mojom::MemoryMapOption, const std::vector<base::ProcessId>& pids,
             MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            base::flat_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
            results[1] = mojom::RawOSMemDump::New();
            results[1]->resident_set_kb = 1;
            results[1]->platform_private_footprint =
                mojom::PlatformPrivateFootprint::New();
            std::move(callback).Run(true, std::move(results));
          }));
#else
  EXPECT_CALL(browser_client, RequestOSMemoryDumpMock(_, Contains(0), _))
      .WillOnce(Invoke(
          [](mojom::MemoryMapOption, const std::vector<base::ProcessId>& pids,
             MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            base::flat_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
            results[0] = mojom::RawOSMemDump::New();
            results[0]->platform_private_footprint =
                mojom::PlatformPrivateFootprint::New();
            results[0]->resident_set_kb = 1;
            std::move(callback).Run(true, std::move(results));
          }));
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  base::test::TestFuture<bool,
                         memory_instrumentation::mojom::GlobalMemoryDumpPtr>
      result;
  RequestGlobalMemoryDump(MemoryDumpType::kSummaryOnly,
                          MemoryDumpLevelOfDetail::kBackground,
                          MemoryDumpDeterminism::kNone, {"partition_alloc/*"},
                          result.GetCallback());

  // Expect that the dump request succeeds.
  ASSERT_TRUE(std::get<bool>(result.Get()));

  // Verify that the dump has a single "partition_alloc" top-level node, and
  // that the top level dump has children.
  const auto& global_dump =
      std::get<memory_instrumentation::mojom::GlobalMemoryDumpPtr>(
          result.Get());
  ASSERT_EQ(global_dump->process_dumps.size(), 1u);

  const auto& process_dump = *global_dump->process_dumps.begin();
  EXPECT_EQ(process_dump->pid, kBrowserPid);

  const auto& allocator_dumps = process_dump->chrome_allocator_dumps;
  ASSERT_EQ(allocator_dumps.size(), 1u);
  EXPECT_EQ(allocator_dumps.begin()->first, "partition_alloc");
  EXPECT_EQ(allocator_dumps.begin()->second->children.size(), 2u);
}

}  // namespace memory_instrumentation
