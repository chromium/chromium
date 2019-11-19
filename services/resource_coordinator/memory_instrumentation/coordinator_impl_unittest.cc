// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/coordinator_impl.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/trace_event_analyzer.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

ACTION_P(RunClosure, closure) {
  closure.Run();
}

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
    coordinator_.reset(new NiceMock<FakeCoordinatorImpl>);
  }

  void TearDown() override { coordinator_.reset(); }

  void RegisterClientProcess(
      mojo::PendingReceiver<mojom::Coordinator> receiver,
      mojo::PendingRemote<mojom::ClientProcess> client_process,
      mojom::ProcessType process_type,
      base::ProcessId pid) {
    coordinator_->RegisterClientProcess(
        std::move(receiver), std::move(client_process), process_type, pid,
        /*service_name=*/base::nullopt);
  }

  void RequestGlobalMemoryDump(RequestGlobalMemoryDumpCallback callback) {
    RequestGlobalMemoryDump(
        MemoryDumpType::SUMMARY_ONLY, MemoryDumpLevelOfDetail::BACKGROUND,
        MemoryDumpDeterminism::NONE, {}, std::move(callback));
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
        MemoryDumpType::EXPLICITLY_TRIGGERED, MemoryDumpLevelOfDetail::DETAILED,
        MemoryDumpDeterminism::NONE, std::move(callback));
  }

  void GetVmRegionsForHeapProfiler(
      const std::vector<base::ProcessId>& pids,
      GetVmRegionsForHeapProfilerCallback callback) {
    coordinator_->GetVmRegionsForHeapProfiler(pids, std::move(callback));
  }

  void ReduceCoordinatorClientProcessTimeout() {
    coordinator_->set_client_process_timeout(
        base::TimeDelta::FromMilliseconds(5));
  }

 protected:
  std::unique_ptr<NiceMock<FakeCoordinatorImpl>> coordinator_;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
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
          MemoryDumpArgs dump_args{MemoryDumpLevelOfDetail::DETAILED};
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

  // TODO(crbug.com/729950): Remove non const reference here once GMock is
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
    return base::BindRepeating(&MockGlobalMemoryDumpCallback::Run,
                               base::Unretained(this));
  }
};

class MockGlobalMemoryDumpAndAppendToTraceCallback {
 public:
  MockGlobalMemoryDumpAndAppendToTraceCallback() = default;
  MOCK_METHOD2(OnCall, void(bool, uint64_t));

  void Run(bool success, uint64_t dump_guid) { OnCall(success, dump_guid); }

  RequestGlobalMemoryDumpAndAppendToTraceCallback Get() {
    return base::BindRepeating(
        &MockGlobalMemoryDumpAndAppendToTraceCallback::Run,
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
    return base::BindRepeating(&MockGetVmRegionsForHeapProfilerCallback::Run,
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
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  RequestGlobalMemoryDump(callback.Get());
  run_loop.Run();
}

// Issuing two requests will cause the second one to be queued.
TEST_F(CoordinatorImplTest, QueuedRequest) {
  base::RunLoop run_loop;

  // This variable to be static as the lambda below has to convert to a function
  // pointer rather than a functor.
  static base::test::SingleThreadTaskEnvironment* task_environment = nullptr;
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
            task_environment->FastForwardBy(
                base::TimeDelta::FromMilliseconds(10));
            MemoryDumpArgs dump_args{MemoryDumpLevelOfDetail::DETAILED};
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
            MemoryDumpArgs dump_args{MemoryDumpLevelOfDetail::DETAILED};
            auto pmd = std::make_unique<ProcessMemoryDump>(dump_args);
            std::move(callback).Run(true, args.dump_guid, std::move(pmd));
          }));

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(
      callback,
      OnCall(true, Pointee(Field(&mojom::GlobalMemoryDump::process_dumps,
                                 IsEmpty()))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
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
      .WillOnce(RunClosure(run_loop.QuitClosure()));
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
      .WillOnce(RunClosure(run_loop.QuitClosure()));
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
#if defined(OS_LINUX)
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
#endif  // defined(OS_LINUX)

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
      .WillOnce(RunClosure(run_loop.QuitClosure()));
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
      .WillOnce(RunClosure(run_loop.QuitClosure()));
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
        MemoryDumpArgs dump_args{MemoryDumpLevelOfDetail::DETAILED};
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
            MemoryDumpArgs dump_args{MemoryDumpLevelOfDetail::DETAILED};
            auto pmd = std::make_unique<ProcessMemoryDump>(dump_args);
            auto* mad = pmd->CreateAllocatorDump(
                "malloc", base::trace_event::MemoryAllocatorDumpGuid(2));
            mad->AddScalar(MemoryAllocatorDump::kNameSize,
                           MemoryAllocatorDump::kUnitsBytes, 1024 * 2);
            std::move(callback).Run(true, args.dump_guid, std::move(pmd));
          }));
#if defined(OS_LINUX)
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
#endif  // defined(OS_LINUX)

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
#if defined(OS_LINUX)
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
#endif  // defined(OS_LINUX)

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
  using trace_analyzer::Query;

  base::RunLoop run_loop;

  NiceMock<MockClientProcess> client_process(this, 1,
                                             mojom::ProcessType::BROWSER);

  EXPECT_CALL(client_process, RequestChromeMemoryDumpMock(_, _))
      .WillOnce(Invoke(
          [](const MemoryDumpRequestArgs& args,
             MockClientProcess::RequestChromeMemoryDumpCallback& callback) {
            MemoryDumpArgs dump_args{MemoryDumpLevelOfDetail::DETAILED};
            auto pmd = std::make_unique<ProcessMemoryDump>(dump_args);
            std::move(callback).Run(true, args.dump_guid, std::move(pmd));
          }));

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(
      callback,
      OnCall(true, Pointee(Field(&mojom::GlobalMemoryDump::process_dumps,
                                 IsEmpty()))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  trace_analyzer::Start(MemoryDumpManager::kTraceCategory);
  RequestGlobalMemoryDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                          MemoryDumpLevelOfDetail::DETAILED,
                          MemoryDumpDeterminism::NONE, {}, callback.Get());
  run_loop.Run();
  auto analyzer = trace_analyzer::Stop();

  trace_analyzer::TraceEventVector events;
  analyzer->FindEvents(
      trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_MEMORY_DUMP),
      &events);

  ASSERT_EQ(0u, events.size());
}

TEST_F(CoordinatorImplTest, DumpsAreAddedToTraceWhenRequested) {
  using trace_analyzer::Query;

  base::RunLoop run_loop;

  NiceMock<MockClientProcess> client_process(this, 1,
                                             mojom::ProcessType::BROWSER);
  EXPECT_CALL(client_process, RequestChromeMemoryDumpMock(_, _))
      .WillOnce(Invoke(
          [](const MemoryDumpRequestArgs& args,
             MockClientProcess::RequestChromeMemoryDumpCallback& callback) {
            MemoryDumpArgs dump_args{MemoryDumpLevelOfDetail::DETAILED};
            auto pmd = std::make_unique<ProcessMemoryDump>(dump_args);
            std::move(callback).Run(true, args.dump_guid, std::move(pmd));
          }));

  MockGlobalMemoryDumpAndAppendToTraceCallback callback;
  EXPECT_CALL(callback, OnCall(true, Ne(0ul)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  trace_analyzer::Start(MemoryDumpManager::kTraceCategory);
  RequestGlobalMemoryDumpAndAppendToTrace(callback.Get());
  run_loop.Run();
  auto analyzer = trace_analyzer::Stop();

  trace_analyzer::TraceEventVector events;
  analyzer->FindEvents(
      trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_MEMORY_DUMP),
      &events);

  ASSERT_EQ(1u, events.size());
  ASSERT_TRUE(trace_analyzer::CountMatches(
      events, trace_analyzer::Query::EventNameIs(MemoryDumpTypeToString(
                  MemoryDumpType::EXPLICITLY_TRIGGERED))));
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
#if defined(OS_LINUX)
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
#endif  // defined(OS_LINUX)

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
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  RequestGlobalMemoryDumpForPid(2, {}, callback.Get());
  run_loop.Run();
}

}  // namespace memory_instrumentation
