// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/raw_ref.h"
#include "base/memory_coordinator/traits.h"
#include "base/test/task_environment.h"
#include "content/common/memory_coordinator/memory_consumer_group_host.h"
#include "content/common/memory_coordinator/memory_coordinator_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using ::testing::_;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Test;

constexpr base::MemoryConsumerTraits kTestTraits1{
    .estimated_memory_usage =
        base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
    .release_memory_cost = base::MemoryConsumerTraits::ReleaseMemoryCost::
        kFreesPagesWithoutTraversal,
    .execution_type = base::MemoryConsumerTraits::ExecutionType::kSynchronous};

class MockPolicy : public MemoryCoordinatorPolicy {
 public:
  explicit MockPolicy(MemoryCoordinatorPolicyManager& manager)
      : MemoryCoordinatorPolicy(manager) {}

  MOCK_METHOD(void,
              OnConsumerGroupAdded,
              (std::string_view consumer_id,
               base::MemoryConsumerTraits traits,
               ProcessType process_type,
               ChildProcessId child_process_id),
              (override));
  MOCK_METHOD(void,
              OnConsumerGroupRemoved,
              (std::string_view consumer_id, ChildProcessId child_process_id),
              (override));

  using MemoryCoordinatorPolicy::manager;
};

class MockMemoryConsumerGroupHost : public MemoryConsumerGroupHost {
 public:
  MOCK_METHOD(void,
              UpdateMemoryLimit,
              (std::string_view consumer_id, int percentage),
              (override));
  MOCK_METHOD(void, ReleaseMemory, (std::string_view consumer_id), (override));
};

}  // namespace

class MemoryCoordinatorPolicyManagerTest : public Test {
 protected:
  MemoryCoordinatorPolicyManager& policy_manager() { return policy_manager_; }

 private:
  base::test::TaskEnvironment task_environment_;
  MemoryCoordinatorPolicyManager policy_manager_;
};

TEST_F(MemoryCoordinatorPolicyManagerTest, PolicyNotification) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(kChildId, &host);

  MockPolicy policy(policy_manager());
  policy_manager().AddPolicy(&policy);

  static constexpr char kConsumerId[] = "consumer";

  EXPECT_CALL(policy, OnConsumerGroupAdded(kConsumerId, kTestTraits1,
                                           PROCESS_TYPE_RENDERER, kChildId));
  policy_manager().OnConsumerGroupAdded(kConsumerId, kTestTraits1,
                                        PROCESS_TYPE_RENDERER, kChildId);
  Mock::VerifyAndClearExpectations(&policy);

  EXPECT_CALL(policy, OnConsumerGroupRemoved(kConsumerId, kChildId));
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  Mock::VerifyAndClearExpectations(&policy);

  // Clean up.
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
  policy_manager().RemovePolicy(&policy);
}

TEST_F(MemoryCoordinatorPolicyManagerTest, AddPolicyNotifiesExistingGroups) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(kChildId, &host);

  static constexpr char kConsumerId[] = "consumer";

  // Add a consumer group BEFORE adding the policy.
  policy_manager().OnConsumerGroupAdded(kConsumerId, kTestTraits1,
                                        PROCESS_TYPE_RENDERER, kChildId);

  MockPolicy policy(policy_manager());

  // Adding the policy should trigger notification of the existing group.
  EXPECT_CALL(policy, OnConsumerGroupAdded(kConsumerId, kTestTraits1,
                                           PROCESS_TYPE_RENDERER, kChildId));
  policy_manager().AddPolicy(&policy);
  Mock::VerifyAndClearExpectations(&policy);

  // Clean up.
  EXPECT_CALL(policy, OnConsumerGroupRemoved(kConsumerId, kChildId));
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
  policy_manager().RemovePolicy(&policy);
}

TEST_F(MemoryCoordinatorPolicyManagerTest, AggregateMemoryLimit) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(kChildId, &host);

  NiceMock<MockPolicy> policy1(policy_manager());
  policy_manager().AddPolicy(&policy1);

  NiceMock<MockPolicy> policy2(policy_manager());
  policy_manager().AddPolicy(&policy2);

  static constexpr char kConsumerId[] = "consumer";

  policy_manager().OnConsumerGroupAdded(kConsumerId, kTestTraits1,
                                        PROCESS_TYPE_RENDERER, kChildId);

  // Both policies request a limit. The minimum should be taken.
  // Initial limit is 100%. Changes to 80%.
  EXPECT_CALL(host, UpdateMemoryLimit(kConsumerId, 80));
  policy1.manager().UpdateMemoryLimit(&policy1, kConsumerId, kChildId, 80);
  Mock::VerifyAndClearExpectations(&host);

  // Changes from 80% to 50%.
  EXPECT_CALL(host, UpdateMemoryLimit(kConsumerId, 50));
  policy2.manager().UpdateMemoryLimit(&policy2, kConsumerId, kChildId, 50);
  Mock::VerifyAndClearExpectations(&host);

  // Updating policy1 with a higher limit should still keep policy2's lower
  // limit. Changes from 50% to 50% (no call).
  EXPECT_CALL(host, UpdateMemoryLimit(_, _)).Times(0);
  policy1.manager().UpdateMemoryLimit(&policy1, kConsumerId, ChildProcessId(1),
                                      90);
  Mock::VerifyAndClearExpectations(&host);

  // Updating policy2 with a higher limit should now use policy1's limit.
  // Changes from 50% to 90%.
  EXPECT_CALL(host, UpdateMemoryLimit(kConsumerId, 90));
  policy2.manager().UpdateMemoryLimit(&policy2, kConsumerId, kChildId, 100);
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
  policy_manager().RemovePolicy(&policy1);
  policy_manager().RemovePolicy(&policy2);
}

TEST_F(MemoryCoordinatorPolicyManagerTest, RemovePolicyClearsData) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(kChildId, &host);

  NiceMock<MockPolicy> policy1(policy_manager());
  policy_manager().AddPolicy(&policy1);

  NiceMock<MockPolicy> policy2(policy_manager());
  policy_manager().AddPolicy(&policy2);

  static constexpr char kConsumerId[] = "consumer";

  policy_manager().OnConsumerGroupAdded(kConsumerId, kTestTraits1,
                                        PROCESS_TYPE_RENDERER, kChildId);

  // policy1 requests 50%. Changes from 100% to 50%.
  EXPECT_CALL(host, UpdateMemoryLimit(kConsumerId, 50));
  policy1.manager().UpdateMemoryLimit(&policy1, kConsumerId, kChildId, 50);
  Mock::VerifyAndClearExpectations(&host);

  // policy2 requests 80%. Does not change from 50% (no call).
  EXPECT_CALL(host, UpdateMemoryLimit(_, _)).Times(0);
  policy2.manager().UpdateMemoryLimit(&policy2, kConsumerId, kChildId, 80);
  Mock::VerifyAndClearExpectations(&host);

  // Removing policy1 should clear its 50% request, so the limit should become
  // 80% (from policy2). Changes from 50% to 80%.
  EXPECT_CALL(host, UpdateMemoryLimit(kConsumerId, 80));
  policy_manager().RemovePolicy(&policy1);
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
  policy_manager().RemovePolicy(&policy2);
}

TEST_F(MemoryCoordinatorPolicyManagerTest, ReleaseMemory) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(kChildId, &host);

  NiceMock<MockPolicy> policy(policy_manager());
  policy_manager().AddPolicy(&policy);

  static constexpr char kConsumerId[] = "consumer";

  policy_manager().OnConsumerGroupAdded(kConsumerId, kTestTraits1,
                                        PROCESS_TYPE_RENDERER, kChildId);

  EXPECT_CALL(host, ReleaseMemory(kConsumerId));
  policy.manager().ReleaseMemory(kConsumerId, kChildId);
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
  policy_manager().RemovePolicy(&policy);
}

TEST_F(MemoryCoordinatorPolicyManagerTest, MultipleProcesses) {
  MockMemoryConsumerGroupHost host1;
  MockMemoryConsumerGroupHost host2;

  const char kConsumerId1[] = "consumer1";
  const char kConsumerId2[] = "consumer2";
  const ChildProcessId kChildId1(1);
  const ChildProcessId kChildId2(2);

  // Register hosts.
  policy_manager().AddMemoryConsumerGroupHost(kChildId1, &host1);
  policy_manager().AddMemoryConsumerGroupHost(kChildId2, &host2);

  // Process 1 adds "consumer1"
  policy_manager().OnConsumerGroupAdded(kConsumerId1, kTestTraits1,
                                        PROCESS_TYPE_RENDERER, kChildId1);

  // Process 2 adds "consumer2"
  policy_manager().OnConsumerGroupAdded(kConsumerId2, kTestTraits1,
                                        PROCESS_TYPE_RENDERER, kChildId2);

  MockPolicy policy(policy_manager());

  EXPECT_CALL(policy, OnConsumerGroupAdded(kConsumerId1, _, _, kChildId1));
  EXPECT_CALL(policy, OnConsumerGroupAdded(kConsumerId2, _, _, kChildId2));
  policy_manager().AddPolicy(&policy);

  // Update limit for both.
  EXPECT_CALL(host1, UpdateMemoryLimit(kConsumerId1, 50));
  EXPECT_CALL(host2, UpdateMemoryLimit(kConsumerId2, 70));
  policy.manager().UpdateMemoryLimit(&policy, kConsumerId1, kChildId1, 50);
  policy.manager().UpdateMemoryLimit(&policy, kConsumerId2, kChildId2, 70);
  Mock::VerifyAndClearExpectations(&host1);
  Mock::VerifyAndClearExpectations(&host2);

  // Release memory for process 1 only.
  EXPECT_CALL(host1, ReleaseMemory(kConsumerId1));
  EXPECT_CALL(host2, ReleaseMemory(_)).Times(0);
  policy.manager().ReleaseMemory(kConsumerId1, kChildId1);
  Mock::VerifyAndClearExpectations(&host1);
  Mock::VerifyAndClearExpectations(&host2);

  EXPECT_CALL(policy, OnConsumerGroupRemoved(kConsumerId1, kChildId1));
  policy_manager().OnConsumerGroupRemoved(kConsumerId1, kChildId1);

  EXPECT_CALL(policy, OnConsumerGroupRemoved(kConsumerId2, kChildId2));
  policy_manager().OnConsumerGroupRemoved(kConsumerId2, kChildId2);

  // Clean up.
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId1);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId2);
  policy_manager().RemovePolicy(&policy);
}

TEST_F(MemoryCoordinatorPolicyManagerTest, SameConsumerIdDifferentChild) {
  MockMemoryConsumerGroupHost host1;
  MockMemoryConsumerGroupHost host2;

  const char kConsumerId[] = "consumer";
  const ChildProcessId kChildId1(1);
  const ChildProcessId kChildId2(2);

  policy_manager().AddMemoryConsumerGroupHost(kChildId1, &host1);
  policy_manager().AddMemoryConsumerGroupHost(kChildId2, &host2);

  // Multiple processes have the same consumer ID.
  policy_manager().OnConsumerGroupAdded(kConsumerId, kTestTraits1,
                                        PROCESS_TYPE_RENDERER, kChildId1);
  policy_manager().OnConsumerGroupAdded(kConsumerId, kTestTraits1,
                                        PROCESS_TYPE_RENDERER, kChildId2);

  MockPolicy policy(policy_manager());

  EXPECT_CALL(policy, OnConsumerGroupAdded(kConsumerId, _, _, kChildId1));
  EXPECT_CALL(policy, OnConsumerGroupAdded(kConsumerId, _, _, kChildId2));
  policy_manager().AddPolicy(&policy);

  // Each group can have its own limit even if they share the same ID.
  EXPECT_CALL(host1, UpdateMemoryLimit(kConsumerId, 40));
  policy.manager().UpdateMemoryLimit(&policy, kConsumerId, kChildId1, 40);

  EXPECT_CALL(host2, UpdateMemoryLimit(kConsumerId, 60));
  policy.manager().UpdateMemoryLimit(&policy, kConsumerId, kChildId2, 60);

  Mock::VerifyAndClearExpectations(&host1);
  Mock::VerifyAndClearExpectations(&host2);

  EXPECT_CALL(policy, OnConsumerGroupRemoved(kConsumerId, kChildId1));
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId1);

  EXPECT_CALL(policy, OnConsumerGroupRemoved(kConsumerId, kChildId2));
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId2);

  // Clean up.
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId1);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId2);
}

TEST_F(MemoryCoordinatorPolicyManagerTest, TestHelpers) {
  MockMemoryConsumerGroupHost host1;
  MockMemoryConsumerGroupHost host2;

  const char kConsumerId1[] = "consumer1";
  const char kConsumerId2[] = "consumer2";
  const ChildProcessId kChildId1(1);
  const ChildProcessId kChildId2(2);

  policy_manager().AddMemoryConsumerGroupHost(kChildId1, &host1);
  policy_manager().AddMemoryConsumerGroupHost(kChildId2, &host2);

  policy_manager().OnConsumerGroupAdded(kConsumerId1, kTestTraits1,
                                        PROCESS_TYPE_RENDERER, kChildId1);
  policy_manager().OnConsumerGroupAdded(kConsumerId2, kTestTraits1,
                                        PROCESS_TYPE_RENDERER, kChildId2);

  EXPECT_CALL(host1, UpdateMemoryLimit(kConsumerId1, 42));
  EXPECT_CALL(host2, UpdateMemoryLimit(kConsumerId2, 42));
  policy_manager().NotifyUpdateMemoryLimitForTesting(42);
  Mock::VerifyAndClearExpectations(&host1);
  Mock::VerifyAndClearExpectations(&host2);

  EXPECT_CALL(host1, ReleaseMemory(kConsumerId1));
  EXPECT_CALL(host2, ReleaseMemory(kConsumerId2));
  policy_manager().NotifyReleaseMemoryForTesting();
  Mock::VerifyAndClearExpectations(&host1);
  Mock::VerifyAndClearExpectations(&host2);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId1, kChildId1);
  policy_manager().OnConsumerGroupRemoved(kConsumerId2, kChildId2);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId1);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId2);
}

TEST_F(MemoryCoordinatorPolicyManagerTest, MultipleConsumersSameChild) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(42);

  policy_manager().AddMemoryConsumerGroupHost(kChildId, &host);

  policy_manager().OnConsumerGroupAdded("consumer1", kTestTraits1,
                                        PROCESS_TYPE_RENDERER, kChildId);
  policy_manager().OnConsumerGroupAdded("consumer2", kTestTraits1,
                                        PROCESS_TYPE_RENDERER, kChildId);

  MockPolicy policy(policy_manager());

  EXPECT_CALL(policy, OnConsumerGroupAdded("consumer1", _, _, kChildId));
  EXPECT_CALL(policy, OnConsumerGroupAdded("consumer2", _, _, kChildId));
  policy_manager().AddPolicy(&policy);

  EXPECT_CALL(host, UpdateMemoryLimit("consumer1", 50));
  EXPECT_CALL(host, UpdateMemoryLimit("consumer2", 80));
  policy.manager().UpdateMemoryLimit(&policy, "consumer1", kChildId, 50);
  policy.manager().UpdateMemoryLimit(&policy, "consumer2", kChildId, 80);

  EXPECT_CALL(policy, OnConsumerGroupRemoved("consumer1", kChildId));
  policy_manager().OnConsumerGroupRemoved("consumer1", kChildId);

  EXPECT_CALL(policy, OnConsumerGroupRemoved("consumer2", kChildId));
  policy_manager().OnConsumerGroupRemoved("consumer2", kChildId);

  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

}  // namespace content
