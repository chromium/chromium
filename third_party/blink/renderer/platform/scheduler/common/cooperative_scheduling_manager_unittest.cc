// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/cooperative_scheduling_manager.h"

#include "base/test/test_mock_time_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {

TEST(CooperativeSchedulingManager, AllowedStackScope) {
  std::unique_ptr<CooperativeSchedulingManager> manager =
      std::make_unique<CooperativeSchedulingManager>();
  {
    EXPECT_FALSE(manager->InAllowedStackScope());
    CooperativeSchedulingManager::AllowedStackScope scope(manager.get());
    EXPECT_TRUE(manager->InAllowedStackScope());
    {
      CooperativeSchedulingManager::AllowedStackScope nested_scope(
          manager.get());
      EXPECT_TRUE(manager->InAllowedStackScope());
    }
    EXPECT_TRUE(manager->InAllowedStackScope());
  }
  EXPECT_FALSE(manager->InAllowedStackScope());
}

class MockCooperativeSchedulingManager : public CooperativeSchedulingManager {
 public:
  MockCooperativeSchedulingManager() : CooperativeSchedulingManager() {
    set_feature_enabled(true);
    ON_CALL(*this, RunNestedLoop())
        .WillByDefault(testing::Invoke(
            this, &MockCooperativeSchedulingManager::RealRunNestedLoop));
  }
  ~MockCooperativeSchedulingManager() override = default;
  MOCK_METHOD0(RunNestedLoop, void());
  void RealRunNestedLoop() { CooperativeSchedulingManager::RunNestedLoop(); }
};

TEST(CooperativeSchedulingManager, SafePoint) {
  {
    std::unique_ptr<MockCooperativeSchedulingManager> manager =
        std::make_unique<MockCooperativeSchedulingManager>();
    EXPECT_CALL(*manager, RunNestedLoop()).Times(0);
    // Should not run nested loop since there is no AllowedStackScope instance.
    manager->Safepoint();
  }
  {
    auto test_task_runner =
        base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    std::unique_ptr<MockCooperativeSchedulingManager> manager =
        std::make_unique<MockCooperativeSchedulingManager>();
    manager->SetTickClockForTesting(test_task_runner->GetMockTickClock());
    CooperativeSchedulingManager::AllowedStackScope scope(manager.get());
    EXPECT_CALL(*manager, RunNestedLoop()).Times(2);
    // Should run nested loop
    manager->Safepoint();
    test_task_runner->FastForwardBy(base::TimeDelta::FromMilliseconds(14));
    // Should not run nested loop because called too soon
    manager->Safepoint();
    test_task_runner->FastForwardBy(base::TimeDelta::FromMilliseconds(2));
    // Should run nested loop
    manager->Safepoint();
  }
}

}  // namespace scheduler
}  // namespace blink
