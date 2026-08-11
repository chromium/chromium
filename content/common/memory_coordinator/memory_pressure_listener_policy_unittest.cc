// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/memory_coordinator/memory_pressure_listener_policy.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "base/hash/hash.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/memory_pressure_listener_registry.h"
#include "base/test/task_environment.h"
#include "content/common/memory_coordinator/memory_consumer_group_host.h"
#include "content/common/memory_coordinator/memory_coordinator_policy.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"
#include "content/public/common/memory_consumer_update.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using ::testing::_;
using ::testing::Mock;
using ::testing::UnorderedElementsAre;

constexpr base::MemoryConsumerTraits kTestTraits(
    base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
    base::MemoryConsumerTraits::ReleaseMemoryCost::kRequiresTraversal,
    base::MemoryConsumerTraits::InformationRetention::kLossless,
    base::MemoryConsumerTraits::ExecutionType::kSynchronous);

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

  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_BROWSER, kChildId,
                                              &host);

  const std::string kConsumerName1 = "consumer1";
  const uint32_t kConsumerId1 = base::PersistentHash(kConsumerName1);
  const std::string kConsumerName2 = "consumer2";
  const uint32_t kConsumerId2 = base::PersistentHash(kConsumerName2);

  policy_manager().OnConsumerGroupAdded(kConsumerId1, kConsumerName1,
                                        kTestTraits, kChildId);
  policy_manager().OnConsumerGroupAdded(kConsumerId2, kConsumerName2,
                                        kTestTraits, kChildId);

  MemoryPressureListenerPolicy policy(policy_manager());
  MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);

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

  policy_manager().OnConsumerGroupRemoved(kConsumerId1, kChildId);
  policy_manager().OnConsumerGroupRemoved(kConsumerId2, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

TEST_F(MemoryPressureListenerPolicyTest, IgnoreOtherProcesses) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kRemoteChildId(1);
  const std::string kRemoteName = "consumer1";
  const uint32_t kRemoteId = base::PersistentHash(kRemoteName);

  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER,
                                              kRemoteChildId, &host);
  policy_manager().OnConsumerGroupAdded(kRemoteId, kRemoteName, kTestTraits,
                                        kRemoteChildId);

  MemoryPressureListenerPolicy policy(policy_manager());
  MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);

  // Local pressure should NOT affect remote process consumers.
  EXPECT_CALL(host, UpdateConsumers(_)).Times(0);
  base::MemoryPressureListener::SimulatePressureNotification(
      base::MEMORY_PRESSURE_LEVEL_MODERATE);
  Mock::VerifyAndClearExpectations(&host);

  policy_manager().OnConsumerGroupRemoved(kRemoteId, kRemoteChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kRemoteChildId);
}

TEST_F(MemoryPressureListenerPolicyTest, Persistence) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId;  // Current process

  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_BROWSER, kChildId,
                                              &host);

  const std::string kConsumerName = "consumer1";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  {
    MemoryPressureListenerPolicy policy(policy_manager());
    MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);

    // Moderate pressure: 50% limit and release memory.
    base::MemoryPressureListener::SimulatePressureNotification(
        base::MEMORY_PRESSURE_LEVEL_MODERATE);

    // A consumer added AFTER the pressure event should immediately receive the
    // limit that was set.
    EXPECT_CALL(host, UpdateConsumers(UnorderedElementsAre(
                          MemoryConsumerUpdate{kConsumerId, 50, true})));
    policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                          kTestTraits, kChildId);
    Mock::VerifyAndClearExpectations(&host);

    // Removing the policy should reset the limit to default (100%).
    EXPECT_CALL(host, UpdateConsumers(UnorderedElementsAre(
                          MemoryConsumerUpdate{kConsumerId, 100, false})));
  }
  Mock::VerifyAndClearExpectations(&host);

  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

}  // namespace content
