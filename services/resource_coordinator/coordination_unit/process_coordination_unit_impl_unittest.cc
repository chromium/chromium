// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"

#include "services/resource_coordinator/coordination_unit/coordination_unit_test_harness.h"
#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/mock_coordination_unit_graphs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {

class ProcessCoordinationUnitImplTest : public CoordinationUnitTestHarness {};

class MockCoordinationUnitGraphObserver : public CoordinationUnitGraphObserver {
 public:
  MockCoordinationUnitGraphObserver() = default;
  virtual ~MockCoordinationUnitGraphObserver() = default;

  bool ShouldObserve(const CoordinationUnitBase* coordination_unit) override {
    return true;
  }

  MOCK_METHOD1(OnAllFramesInProcessFrozen,
               void(const ProcessCoordinationUnitImpl*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCoordinationUnitGraphObserver);
};

}  // namespace

TEST_F(ProcessCoordinationUnitImplTest, MeasureCPUUsage) {
  auto process_cu = CreateCoordinationUnit<ProcessCoordinationUnitImpl>();
  process_cu->SetCPUUsage(1);
  int64_t cpu_usage;
  EXPECT_TRUE(
      process_cu->GetProperty(mojom::PropertyType::kCPUUsage, &cpu_usage));
  EXPECT_EQ(1, cpu_usage / 1000.0);
}

TEST_F(ProcessCoordinationUnitImplTest, OnAllFramesInProcessFrozen) {
  auto owned_observer = std::make_unique<
      testing::StrictMock<MockCoordinationUnitGraphObserver>>();
  auto* observer = owned_observer.get();
  coordination_unit_graph()->RegisterObserver(std::move(owned_observer));
  MockMultiplePagesInSingleProcessCoordinationUnitGraph cu_graph(
      coordination_unit_graph());

  // 1/2 frame in the process is frozen.
  // No call to OnAllFramesInProcessFrozen() is expected.
  cu_graph.frame->SetLifecycleState(mojom::LifecycleState::kFrozen);

  // 2/2 frames in the process are frozen.
  EXPECT_CALL(*observer, OnAllFramesInProcessFrozen(cu_graph.process.get()));
  cu_graph.other_frame->SetLifecycleState(mojom::LifecycleState::kFrozen);
  testing::Mock::VerifyAndClear(observer);

  // A frame is unfrozen and frozen.
  cu_graph.frame->SetLifecycleState(mojom::LifecycleState::kRunning);
  EXPECT_CALL(*observer, OnAllFramesInProcessFrozen(cu_graph.process.get()));
  cu_graph.frame->SetLifecycleState(mojom::LifecycleState::kFrozen);
  testing::Mock::VerifyAndClear(observer);

  // A frozen frame is frozen again.
  // No call to OnAllFramesInProcessFrozen() is expected.
  cu_graph.frame->SetLifecycleState(mojom::LifecycleState::kFrozen);
}

}  // namespace resource_coordinator
