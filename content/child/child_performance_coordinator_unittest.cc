// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_performance_coordinator.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom.h"
#include "components/performance_manager/scenario_api/performance_scenario_test_support.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using performance_manager::mojom::ChildProcessCoordinationUnit;
using performance_scenarios::PerformanceScenarioTestHelper;
using performance_scenarios::ScenarioScope;
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
  void SetUp() override {
    scenario_test_helper_ =
        PerformanceScenarioTestHelper::CreateWithoutMapping();
    ASSERT_TRUE(scenario_test_helper_);
  }

  // Initializes the ChildPerformanceCoordinator and waits for a mock
  // ChildProcessCoordinationUnit to send it `global_region` and
  // `process_region`.
  void InitializeAndWaitForScenarioRegions(
      base::ReadOnlySharedMemoryRegion global_region,
      base::ReadOnlySharedMemoryRegion process_region) {
    base::OnceClosure quit_closure = task_env_.QuitClosure();
    StrictMockChildProcessCoordinationUnit mock_coordination_unit;
    EXPECT_CALL(mock_coordination_unit,
                InitializeChildProcessCoordination(_, _))
        .WillOnce(Invoke(
            [&](uint64_t, InitializeChildProcessCoordinationCallback callback) {
              std::move(callback).Run(std::move(global_region),
                                      std::move(process_region));
              // `callback` will post to ChildPerformanceCoordinator. Quit the
              // runloop after the posted task.
              task_env_.GetMainThreadTaskRunner()->PostTask(
                  FROM_HERE, std::move(quit_closure));
            }));
    mock_coordination_unit.Bind(coordinator_.InitializeAndPassReceiver());
    task_env_.RunUntilQuit();
  }

  PerformanceScenarioTestHelper& scenario_test_helper() {
    return *scenario_test_helper_;
  }

 private:
  base::test::TaskEnvironment task_env_;
  std::unique_ptr<PerformanceScenarioTestHelper> scenario_test_helper_;
  ChildPerformanceCoordinator coordinator_;
};

TEST_F(ChildPerformanceCoordinatorTest, NoScenarioRegion) {
  InitializeAndWaitForScenarioRegions(base::ReadOnlySharedMemoryRegion(),
                                      base::ReadOnlySharedMemoryRegion());

  EXPECT_FALSE(performance_scenarios::GetScenarioMappingForScope(
      ScenarioScope::kGlobal));
  EXPECT_FALSE(performance_scenarios::GetScenarioMappingForScope(
      ScenarioScope::kCurrentProcess));
}

TEST_F(ChildPerformanceCoordinatorTest, GlobalScenarioRegion) {
  InitializeAndWaitForScenarioRegions(
      scenario_test_helper().GetReadOnlyScenarioRegion(ScenarioScope::kGlobal),
      base::ReadOnlySharedMemoryRegion());

  EXPECT_TRUE(performance_scenarios::GetScenarioMappingForScope(
      ScenarioScope::kGlobal));
  EXPECT_FALSE(performance_scenarios::GetScenarioMappingForScope(
      ScenarioScope::kCurrentProcess));
}

TEST_F(ChildPerformanceCoordinatorTest, ProcessScenarioRegion) {
  InitializeAndWaitForScenarioRegions(
      base::ReadOnlySharedMemoryRegion(),
      scenario_test_helper().GetReadOnlyScenarioRegion(
          ScenarioScope::kCurrentProcess));

  EXPECT_FALSE(performance_scenarios::GetScenarioMappingForScope(
      ScenarioScope::kGlobal));
  EXPECT_TRUE(performance_scenarios::GetScenarioMappingForScope(
      ScenarioScope::kCurrentProcess));
}

}  // namespace
}  // namespace content
