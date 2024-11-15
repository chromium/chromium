// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_performance_coordinator.h"

#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/structured_shared_memory.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/performance/performance_scenarios.h"

namespace content {
namespace {

using blink::performance_scenarios::ScenarioScope;
using blink::performance_scenarios::ScenarioState;
using blink::performance_scenarios::ScopedReadOnlyScenarioMemory;
using performance_manager::mojom::ChildProcessCoordinationUnit;
using ::testing::_;
using ::testing::Invoke;

using InitializeChildProcessCoordinationCallback =
    ChildProcessCoordinationUnit::InitializeChildProcessCoordinationCallback;

class MockChildProcessCoordinationUnit : public ChildProcessCoordinationUnit {
 public:
  MOCK_METHOD(void,
              InitializeChildProcessCoordination,
              (uint64_t, InitializeChildProcessCoordinationCallback),
              (override));

  void Bind(mojo::PendingReceiver<ChildProcessCoordinationUnit> receiver) {
    receiver_.Bind(std::move(receiver));
  }

 private:
  mojo::Receiver<ChildProcessCoordinationUnit> receiver_{this};
};

using StrictMockChildProcessCoordinationUnit =
    ::testing::StrictMock<MockChildProcessCoordinationUnit>;

class ChildPerformanceCoordinatorTest : public ::testing::Test {
 public:
  // Initializes `coordinator` and waits for a mock ChildProcessCoordinationUnit
  // to send it `global_region` and `process_region`.
  void InitializeAndWaitForScenarioRegions(
      ChildPerformanceCoordinator& coordinator,
      base::ReadOnlySharedMemoryRegion global_region,
      base::ReadOnlySharedMemoryRegion process_region) {
    global_region_ = std::move(global_region);
    process_region_ = std::move(process_region);
    quit_closure_ = task_env_.QuitClosure();

    StrictMockChildProcessCoordinationUnit mock_coordination_unit;
    EXPECT_CALL(mock_coordination_unit,
                InitializeChildProcessCoordination(_, _))
        .WillOnce(Invoke(
            this,
            &ChildPerformanceCoordinatorTest::SendScenarioRegionsAndQuit));
    mock_coordination_unit.Bind(coordinator.InitializeAndPassReceiver());
    task_env_.RunUntilQuit();
  }

  // Invokes `callback` with the `global_region_` and `process_region_` and
  // quits the run loop.
  void SendScenarioRegionsAndQuit(
      uint64_t,
      InitializeChildProcessCoordinationCallback callback) {
    std::move(callback).Run(std::move(global_region_),
                            std::move(process_region_));
    // `callback` will post to ChildPerformanceCoordinator. Quit the runloop
    // after the posted task.
    task_env_.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                  std::move(quit_closure_));
  }

 private:
  base::test::TaskEnvironment task_env_;

  // State used by SendScenarioRegionsAndQuit.
  base::ReadOnlySharedMemoryRegion global_region_;
  base::ReadOnlySharedMemoryRegion process_region_;
  base::OnceClosure quit_closure_;
};

TEST_F(ChildPerformanceCoordinatorTest, NoScenarioRegion) {
  ChildPerformanceCoordinator coordinator;
  InitializeAndWaitForScenarioRegions(coordinator,
                                      base::ReadOnlySharedMemoryRegion(),
                                      base::ReadOnlySharedMemoryRegion());

  EXPECT_FALSE(ScopedReadOnlyScenarioMemory::GetMappingForTesting(
      ScenarioScope::kGlobal));
  EXPECT_FALSE(ScopedReadOnlyScenarioMemory::GetMappingForTesting(
      ScenarioScope::kCurrentProcess));
}

TEST_F(ChildPerformanceCoordinatorTest, GlobalScenarioRegion) {
  auto shared_memory = base::StructuredSharedMemory<ScenarioState>::Create();
  ASSERT_TRUE(shared_memory.has_value());

  ChildPerformanceCoordinator coordinator;
  InitializeAndWaitForScenarioRegions(coordinator,
                                      shared_memory->TakeReadOnlyRegion(),
                                      base::ReadOnlySharedMemoryRegion());

  EXPECT_TRUE(ScopedReadOnlyScenarioMemory::GetMappingForTesting(
      ScenarioScope::kGlobal));
  EXPECT_FALSE(ScopedReadOnlyScenarioMemory::GetMappingForTesting(
      ScenarioScope::kCurrentProcess));
}

TEST_F(ChildPerformanceCoordinatorTest, ProcessScenarioRegion) {
  auto shared_memory = base::StructuredSharedMemory<ScenarioState>::Create();
  ASSERT_TRUE(shared_memory.has_value());

  ChildPerformanceCoordinator coordinator;
  InitializeAndWaitForScenarioRegions(coordinator,
                                      base::ReadOnlySharedMemoryRegion(),
                                      shared_memory->TakeReadOnlyRegion());

  EXPECT_FALSE(ScopedReadOnlyScenarioMemory::GetMappingForTesting(
      ScenarioScope::kGlobal));
  EXPECT_TRUE(ScopedReadOnlyScenarioMemory::GetMappingForTesting(
      ScenarioScope::kCurrentProcess));
}

}  // namespace
}  // namespace content
