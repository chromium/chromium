// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"

#include <memory>

#include "base/memory_coordinator/mock_memory_consumer.h"
#include "base/memory_coordinator/traits.h"
#include "base/test/task_environment.h"
#include "content/common/memory_coordinator/memory_coordinator_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using ::testing::_;
using ::testing::Mock;
using ::testing::Test;

constexpr base::MemoryConsumerTraits kTestTraits1{
    .estimated_memory_usage =
        base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
    .release_memory_cost = base::MemoryConsumerTraits::ReleaseMemoryCost::
        kFreesPagesWithoutTraversal,
    .execution_type = base::MemoryConsumerTraits::ExecutionType::kSynchronous};

class SimpleMemoryConsumerRegistry : public base::MemoryConsumerRegistry {
 public:
  explicit SimpleMemoryConsumerRegistry(
      MemoryConsumerGroupController& controller)
      : controller_(controller) {}
  ~SimpleMemoryConsumerRegistry() override { NotifyDestruction(); }

 private:
  void OnMemoryConsumerAdded(std::string_view consumer_id,
                             base::MemoryConsumerTraits traits,
                             base::RegisteredMemoryConsumer consumer) override {
    controller_->OnConsumerGroupAdded(consumer_id, traits, PROCESS_TYPE_BROWSER,
                                      ChildProcessId(), consumer);
  }

  void OnMemoryConsumerRemoved(
      std::string_view consumer_id,
      base::RegisteredMemoryConsumer consumer) override {
    controller_->OnConsumerGroupRemoved(consumer_id, ChildProcessId());
  }

  const raw_ref<MemoryConsumerGroupController> controller_;
};

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

}  // namespace

class MemoryCoordinatorPolicyManagerTest : public Test {
 protected:
  MemoryCoordinatorPolicyManager& policy_manager() { return policy_manager_; }
  SimpleMemoryConsumerRegistry& registry() { return registry_.Get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  MemoryCoordinatorPolicyManager policy_manager_;
  base::ScopedMemoryConsumerRegistry<SimpleMemoryConsumerRegistry> registry_{
      policy_manager_};
};

TEST_F(MemoryCoordinatorPolicyManagerTest, PolicyNotification) {
  MockPolicy policy(policy_manager());
  policy_manager().AddPolicy(&policy);

  base::MockMemoryConsumer consumer;
  static constexpr char kConsumerId[] = "consumer";

  EXPECT_CALL(policy, OnConsumerGroupAdded(kConsumerId, kTestTraits1,
                                           PROCESS_TYPE_BROWSER, _));
  registry().AddMemoryConsumer(kConsumerId, kTestTraits1, &consumer);
  Mock::VerifyAndClearExpectations(&policy);

  EXPECT_CALL(policy, OnConsumerGroupRemoved(kConsumerId, _));
  registry().RemoveMemoryConsumer(kConsumerId, &consumer);
  Mock::VerifyAndClearExpectations(&policy);

  policy_manager().RemovePolicy(&policy);
}

TEST_F(MemoryCoordinatorPolicyManagerTest, AddPolicyNotifiesExistingGroups) {
  base::MockMemoryConsumer consumer;
  static constexpr char kConsumerId[] = "consumer";

  // Add a consumer group BEFORE adding the policy.
  registry().AddMemoryConsumer(kConsumerId, kTestTraits1, &consumer);

  MockPolicy policy(policy_manager());

  // Adding the policy should trigger notification of the existing group.
  EXPECT_CALL(policy, OnConsumerGroupAdded(kConsumerId, kTestTraits1,
                                           PROCESS_TYPE_BROWSER, _));
  policy_manager().AddPolicy(&policy);
  Mock::VerifyAndClearExpectations(&policy);

  registry().RemoveMemoryConsumer(kConsumerId, &consumer);
  policy_manager().RemovePolicy(&policy);
}

TEST_F(MemoryCoordinatorPolicyManagerTest, AggregateMemoryLimit) {
  MockPolicy policy1(policy_manager());
  policy_manager().AddPolicy(&policy1);

  MockPolicy policy2(policy_manager());
  policy_manager().AddPolicy(&policy2);

  base::MockMemoryConsumer consumer;
  static constexpr char kConsumerId[] = "consumer";

  registry().AddMemoryConsumer(kConsumerId, kTestTraits1, &consumer);

  // Both policies request a limit. The minimum should be taken.
  EXPECT_CALL(consumer, OnUpdateMemoryLimit());
  policy1.manager().SetMemoryLimit(&policy1, kConsumerId, ChildProcessId(), 80);
  EXPECT_EQ(consumer.memory_limit(), 80);
  Mock::VerifyAndClearExpectations(&consumer);

  EXPECT_CALL(consumer, OnUpdateMemoryLimit());
  policy2.manager().SetMemoryLimit(&policy2, kConsumerId, ChildProcessId(), 50);
  EXPECT_EQ(consumer.memory_limit(), 50);
  Mock::VerifyAndClearExpectations(&consumer);

  // Updating policy1 with a higher limit should still keep policy2's lower
  // limit.
  EXPECT_CALL(consumer, OnUpdateMemoryLimit());
  policy1.manager().SetMemoryLimit(&policy1, kConsumerId, ChildProcessId(), 90);
  EXPECT_EQ(consumer.memory_limit(), 50);
  Mock::VerifyAndClearExpectations(&consumer);

  // Updating policy2 with a higher limit should now use policy1's limit.
  EXPECT_CALL(consumer, OnUpdateMemoryLimit());
  policy2.manager().SetMemoryLimit(&policy2, kConsumerId, ChildProcessId(),
                                   100);
  EXPECT_EQ(consumer.memory_limit(), 90);
  Mock::VerifyAndClearExpectations(&consumer);

  registry().RemoveMemoryConsumer(kConsumerId, &consumer);
  policy_manager().RemovePolicy(&policy1);
  policy_manager().RemovePolicy(&policy2);
}

TEST_F(MemoryCoordinatorPolicyManagerTest, RemovePolicyClearsData) {
  MockPolicy policy1(policy_manager());
  policy_manager().AddPolicy(&policy1);

  MockPolicy policy2(policy_manager());
  policy_manager().AddPolicy(&policy2);

  base::MockMemoryConsumer consumer;
  static constexpr char kConsumerId[] = "consumer";

  registry().AddMemoryConsumer(kConsumerId, kTestTraits1, &consumer);

  // policy1 requests 50%, policy2 requests 80%. Limit should be 50%.
  policy1.manager().SetMemoryLimit(&policy1, kConsumerId, ChildProcessId(), 50);
  policy2.manager().SetMemoryLimit(&policy2, kConsumerId, ChildProcessId(), 80);
  EXPECT_EQ(consumer.memory_limit(), 50);

  // Removing policy1 should clear its 50% request, so the limit should become
  // 80% (from policy2).
  EXPECT_CALL(consumer, OnUpdateMemoryLimit());
  policy_manager().RemovePolicy(&policy1);
  EXPECT_EQ(consumer.memory_limit(), 80);
  Mock::VerifyAndClearExpectations(&consumer);

  registry().RemoveMemoryConsumer(kConsumerId, &consumer);
  policy_manager().RemovePolicy(&policy2);
}

TEST_F(MemoryCoordinatorPolicyManagerTest, ReleaseMemory) {
  MockPolicy policy(policy_manager());
  policy_manager().AddPolicy(&policy);

  base::MockMemoryConsumer consumer;
  static constexpr char kConsumerId[] = "consumer";

  registry().AddMemoryConsumer(kConsumerId, kTestTraits1, &consumer);

  EXPECT_CALL(consumer, OnReleaseMemory());
  policy.manager().ReleaseMemory(kConsumerId, ChildProcessId());
  Mock::VerifyAndClearExpectations(&consumer);

  registry().RemoveMemoryConsumer(kConsumerId, &consumer);
  policy_manager().RemovePolicy(&policy);
}

}  // namespace content
