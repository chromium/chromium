// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/memory_coordinator/memory_pressure_listener_policy.h"

#include <memory>
#include <vector>

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/memory_pressure_listener_registry.h"
#include "base/test/task_environment.h"
#include "content/common/memory_coordinator/memory_consumer_group_host.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"
#include "content/public/common/memory_consumer_update.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using ::testing::_;
using ::testing::Mock;
using ::testing::UnorderedElementsAre;

class MockMemoryConsumerGroupHost : public MemoryConsumerGroupHost {
 public:
  MOCK_METHOD(void,
              UpdateConsumers,
              (std::vector<MemoryConsumerUpdate> updates),
              (override));
};

}  // namespace

class MemoryPressureListenerPolicyTest : public testing::Test {
 protected:
  MemoryCoordinatorPolicyManager& policy_manager() { return policy_manager_; }

 private:
  base::test::TaskEnvironment task_environment_;
  base::MemoryPressureListenerRegistry registry_;
  MemoryCoordinatorPolicyManager policy_manager_;
};
TEST_F(MemoryPressureListenerPolicyTest, ResponseToPressure) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId;  // Current process

  policy_manager().AddMemoryConsumerGroupHost(kChildId, &host);

  const std::string kConsumerId1 = "consumer1";
  const std::string kConsumerId2 = "consumer2";

  policy_manager().OnConsumerGroupAdded(kConsumerId1, {}, PROCESS_TYPE_BROWSER,
                                        kChildId);
  policy_manager().OnConsumerGroupAdded(kConsumerId2, {}, PROCESS_TYPE_BROWSER,
                                        kChildId);

  MemoryPressureListenerPolicy policy(policy_manager());
  policy_manager().AddPolicy(&policy);

  // Moderate pressure: 50% limit and release memory.
  EXPECT_CALL(host, UpdateConsumers(UnorderedElementsAre(
                        MemoryConsumerUpdate{kConsumerId1, 50, true},
                        MemoryConsumerUpdate{kConsumerId2, 50, true})));
  base::MemoryPressureListener::SimulatePressureNotification(
      base::MEMORY_PRESSURE_LEVEL_MODERATE);
  Mock::VerifyAndClearExpectations(&host);

  // Critical pressure: 0% limit and release memory.
  EXPECT_CALL(host, UpdateConsumers(UnorderedElementsAre(
                        MemoryConsumerUpdate{kConsumerId1, 0, true},
                        MemoryConsumerUpdate{kConsumerId2, 0, true})));
  base::MemoryPressureListener::SimulatePressureNotification(
      base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  Mock::VerifyAndClearExpectations(&host);

  // No pressure: 100% limit and release memory.
  EXPECT_CALL(host, UpdateConsumers(UnorderedElementsAre(
                        MemoryConsumerUpdate{kConsumerId1, 100, true},
                        MemoryConsumerUpdate{kConsumerId2, 100, true})));
  base::MemoryPressureListener::SimulatePressureNotification(
      base::MEMORY_PRESSURE_LEVEL_NONE);
  Mock::VerifyAndClearExpectations(&host);

  policy_manager().RemovePolicy(&policy);
  policy_manager().OnConsumerGroupRemoved(kConsumerId1, kChildId);
  policy_manager().OnConsumerGroupRemoved(kConsumerId2, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

TEST_F(MemoryPressureListenerPolicyTest, IgnoreOtherProcesses) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kRemoteChildId(1);

  policy_manager().AddMemoryConsumerGroupHost(kRemoteChildId, &host);
  policy_manager().OnConsumerGroupAdded("remote", {}, PROCESS_TYPE_RENDERER,
                                        kRemoteChildId);

  MemoryPressureListenerPolicy policy(policy_manager());
  policy_manager().AddPolicy(&policy);

  // Local pressure should NOT affect remote process consumers.
  EXPECT_CALL(host, UpdateConsumers(_)).Times(0);
  base::MemoryPressureListener::SimulatePressureNotification(
      base::MEMORY_PRESSURE_LEVEL_MODERATE);
  Mock::VerifyAndClearExpectations(&host);

  policy_manager().RemovePolicy(&policy);
  policy_manager().OnConsumerGroupRemoved("remote", kRemoteChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kRemoteChildId);
}

}  // namespace content
