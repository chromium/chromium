// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/memory_coordinator/traits.h"
#include "base/test/task_environment.h"
#include "content/common/buildflags.h"
#include "content/common/memory_coordinator/memory_consumer_group_host.h"
#include "content/common/memory_coordinator/memory_coordinator_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using ::testing::_;
using ::testing::ElementsAre;
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

  using MemoryCoordinatorPolicy::manager;
};

class MockMemoryConsumerGroupHost : public MemoryConsumerGroupHost {
 public:
  MOCK_METHOD(void,
              UpdateConsumers,
              (std::vector<MemoryConsumerUpdate> updates),
              (override));
};

}  // namespace

class MemoryCoordinatorPolicyManagerTest : public Test {
 protected:
  MemoryCoordinatorPolicyManager& policy_manager() { return policy_manager_; }

 private:
  base::test::TaskEnvironment task_environment_;
  MemoryCoordinatorPolicyManager policy_manager_;
};

TEST_F(MemoryCoordinatorPolicyManagerTest, AggregateMemoryLimit) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(kChildId, &host);

  NiceMock<MockPolicy> policy1(policy_manager());
  MemoryCoordinatorPolicyRegistration reg1(policy_manager(), policy1);

  NiceMock<MockPolicy> policy2(policy_manager());
  MemoryCoordinatorPolicyRegistration reg2(policy_manager(), policy2);

  static constexpr char kConsumerId[] = "consumer";

  policy_manager().OnConsumerGroupAdded(kConsumerId, kTestTraits1,
                                        PROCESS_TYPE_RENDERER, kChildId);

  // Both policies request a limit. The minimum should be taken.
  // Initial limit is 100%. Changes to 80%.
  EXPECT_CALL(host, UpdateConsumers(ElementsAre(
                        MemoryConsumerUpdate{kConsumerId, 80, false})));
  policy1.manager().UpdateConsumers(&policy1,
                                    {{kChildId, {kConsumerId, 80, false}}});
  Mock::VerifyAndClearExpectations(&host);

  // Changes from 80% to 50%.
  EXPECT_CALL(host, UpdateConsumers(ElementsAre(
                        MemoryConsumerUpdate{kConsumerId, 50, false})));
  policy2.manager().UpdateConsumers(&policy2,
                                    {{kChildId, {kConsumerId, 50, false}}});
  Mock::VerifyAndClearExpectations(&host);

  // Updating policy1 with a higher limit should still keep policy2's lower
  // limit. Changes from 50% to 50% (no call).
  EXPECT_CALL(host, UpdateConsumers(_)).Times(0);
  policy1.manager().UpdateConsumers(&policy1,
                                    {{kChildId, {kConsumerId, 90, false}}});
  Mock::VerifyAndClearExpectations(&host);

  // Updating policy2 with a higher limit should now use policy1's limit.
  // Changes from 50% to 90%.
  EXPECT_CALL(host, UpdateConsumers(ElementsAre(
                        MemoryConsumerUpdate{kConsumerId, 90, false})));
  policy2.manager().UpdateConsumers(&policy2,
                                    {{kChildId, {kConsumerId, 100, false}}});
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

TEST_F(MemoryCoordinatorPolicyManagerTest, RemovePolicyClearsData) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(kChildId, &host);

  NiceMock<MockPolicy> policy1(policy_manager());
  auto reg1 = std::make_unique<MemoryCoordinatorPolicyRegistration<MockPolicy>>(
      policy_manager(), policy1);

  NiceMock<MockPolicy> policy2(policy_manager());
  MemoryCoordinatorPolicyRegistration reg2(policy_manager(), policy2);

  static constexpr char kConsumerId[] = "consumer";

  policy_manager().OnConsumerGroupAdded(kConsumerId, kTestTraits1,
                                        PROCESS_TYPE_RENDERER, kChildId);

  // policy1 requests 50%. Changes from 100% to 50%.
  EXPECT_CALL(host, UpdateConsumers(ElementsAre(
                        MemoryConsumerUpdate{kConsumerId, 50, false})));
  policy1.manager().UpdateConsumers(&policy1,
                                    {{kChildId, {kConsumerId, 50, false}}});
  Mock::VerifyAndClearExpectations(&host);

  // policy2 requests 80%. Does not change from 50% (no call).
  EXPECT_CALL(host, UpdateConsumers(_)).Times(0);
  policy2.manager().UpdateConsumers(&policy2,
                                    {{kChildId, {kConsumerId, 80, false}}});
  Mock::VerifyAndClearExpectations(&host);

  // Removing policy1 should clear its 50% request, so the limit should become
  // 80% (from policy2). Changes from 50% to 80%.
  EXPECT_CALL(host, UpdateConsumers(ElementsAre(
                        MemoryConsumerUpdate{kConsumerId, 80, false})));
  reg1.reset();
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

TEST_F(MemoryCoordinatorPolicyManagerTest, ReleaseMemory) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(kChildId, &host);

  NiceMock<MockPolicy> policy(policy_manager());
  MemoryCoordinatorPolicyRegistration reg(policy_manager(), policy);

  static constexpr char kConsumerId[] = "consumer";

  policy_manager().OnConsumerGroupAdded(kConsumerId, kTestTraits1,
                                        PROCESS_TYPE_RENDERER, kChildId);

  EXPECT_CALL(host, UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                        kConsumerId, std::nullopt, true})));
  policy.manager().UpdateConsumers(
      &policy, {{kChildId, {kConsumerId, std::nullopt, true}}});
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
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

  EXPECT_CALL(host1, UpdateConsumers(ElementsAre(
                         MemoryConsumerUpdate{kConsumerId1, 42, false})));
  EXPECT_CALL(host2, UpdateConsumers(ElementsAre(
                         MemoryConsumerUpdate{kConsumerId2, 42, false})));
  policy_manager().NotifyUpdateMemoryLimitForTesting(42);
  Mock::VerifyAndClearExpectations(&host1);
  Mock::VerifyAndClearExpectations(&host2);

  EXPECT_CALL(host1, UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                         kConsumerId1, std::nullopt, true})));
  EXPECT_CALL(host2, UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                         kConsumerId2, std::nullopt, true})));
  policy_manager().NotifyReleaseMemoryForTesting();
  Mock::VerifyAndClearExpectations(&host1);
  Mock::VerifyAndClearExpectations(&host2);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId1, kChildId1);
  policy_manager().OnConsumerGroupRemoved(kConsumerId2, kChildId2);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId1);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId2);
}

TEST_F(MemoryCoordinatorPolicyManagerTest, UpdateConsumers_MultipleProcesses) {
  MockMemoryConsumerGroupHost host1;
  MockMemoryConsumerGroupHost host2;
  const ChildProcessId kChildId1(1);
  const ChildProcessId kChildId2(2);

  policy_manager().AddMemoryConsumerGroupHost(kChildId1, &host1);
  policy_manager().AddMemoryConsumerGroupHost(kChildId2, &host2);

  policy_manager().OnConsumerGroupAdded("consumer1", kTestTraits1,
                                        PROCESS_TYPE_RENDERER, kChildId1);
  policy_manager().OnConsumerGroupAdded("consumer2", kTestTraits1,
                                        PROCESS_TYPE_RENDERER, kChildId2);

  MockPolicy policy(policy_manager());
  MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);

  EXPECT_CALL(host1, UpdateConsumers(ElementsAre(
                         MemoryConsumerUpdate{"consumer1", 50, true})));
  EXPECT_CALL(host2, UpdateConsumers(ElementsAre(
                         MemoryConsumerUpdate{"consumer2", 80, false})));

  policy.manager().UpdateConsumers(&policy,
                                   {{kChildId1, {"consumer1", 50, true}},
                                    {kChildId2, {"consumer2", 80, false}}});

  Mock::VerifyAndClearExpectations(&host1);
  Mock::VerifyAndClearExpectations(&host2);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved("consumer1", kChildId1);
  policy_manager().OnConsumerGroupRemoved("consumer2", kChildId2);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId1);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId2);
}

TEST_F(MemoryCoordinatorPolicyManagerTest, UpdateConsumers_Filter) {
  MockMemoryConsumerGroupHost host1;
  MockMemoryConsumerGroupHost host2;
  const ChildProcessId kChildId1(1);
  const ChildProcessId kChildId2(2);

  policy_manager().AddMemoryConsumerGroupHost(kChildId1, &host1);
  policy_manager().AddMemoryConsumerGroupHost(kChildId2, &host2);

  const base::MemoryConsumerTraits kTraits1{
      .supports_memory_limit =
          base::MemoryConsumerTraits::SupportsMemoryLimit::kYes};
  const base::MemoryConsumerTraits kTraits2{
      .supports_memory_limit =
          base::MemoryConsumerTraits::SupportsMemoryLimit::kNo};

  policy_manager().OnConsumerGroupAdded("consumer1", kTraits1,
                                        PROCESS_TYPE_RENDERER, kChildId1);
  policy_manager().OnConsumerGroupAdded("consumer2", kTraits2,
                                        PROCESS_TYPE_RENDERER, kChildId2);

  MockPolicy policy(policy_manager());
  MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);

  // Update only consumers with kTraits1.
  EXPECT_CALL(host1, UpdateConsumers(ElementsAre(
                         MemoryConsumerUpdate{"consumer1", 50, true})));
  EXPECT_CALL(host2, UpdateConsumers(_)).Times(0);

  policy.manager().UpdateConsumers(
      &policy,
      [](std::string_view consumer_id,
         std::optional<base::MemoryConsumerTraits> traits,
         ProcessType process_type, ChildProcessId child_process_id) {
        return traits.has_value() &&
               traits->supports_memory_limit ==
                   base::MemoryConsumerTraits::SupportsMemoryLimit::kYes;
      },
      50, true);

  Mock::VerifyAndClearExpectations(&host1);
  Mock::VerifyAndClearExpectations(&host2);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved("consumer1", kChildId1);
  policy_manager().OnConsumerGroupRemoved("consumer2", kChildId2);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId1);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId2);
}

namespace {

class MockObserverPolicy : public MemoryCoordinatorPolicy,
                           public MemoryCoordinatorPolicyManager::Observer {
 public:
  explicit MockObserverPolicy(MemoryCoordinatorPolicyManager& manager)
      : MemoryCoordinatorPolicy(manager) {}

  MOCK_METHOD(void,
              OnConsumerGroupAdded,
              (std::string_view consumer_id,
               std::optional<base::MemoryConsumerTraits> traits,
               ProcessType process_type,
               ChildProcessId child_process_id),
              (override));
  MOCK_METHOD(void,
              OnConsumerGroupRemoved,
              (std::string_view consumer_id, ChildProcessId child_process_id),
              (override));

  using MemoryCoordinatorPolicy::manager;
};

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
class MockDiagnosticObserver
    : public MemoryCoordinatorPolicyManager::DiagnosticObserver {
 public:
  MOCK_METHOD(void,
              OnMemoryLimitChanged,
              (std::string_view consumer_id,
               ChildProcessId child_process_id,
               int memory_limit),
              (override));
};
#endif

}  // namespace

class MemoryCoordinatorPolicyObserverTest
    : public MemoryCoordinatorPolicyManagerTest {};

TEST_F(MemoryCoordinatorPolicyObserverTest, PolicyNotification) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(kChildId, &host);

  MockObserverPolicy policy(policy_manager());
  MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);

  static constexpr char kConsumerId[] = "consumer";

  EXPECT_CALL(policy, OnConsumerGroupAdded(kConsumerId,
                                           std::make_optional(kTestTraits1),
                                           PROCESS_TYPE_RENDERER, kChildId));
  policy_manager().OnConsumerGroupAdded(kConsumerId, kTestTraits1,
                                        PROCESS_TYPE_RENDERER, kChildId);
  Mock::VerifyAndClearExpectations(&policy);

  EXPECT_CALL(policy, OnConsumerGroupRemoved(kConsumerId, kChildId));
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  Mock::VerifyAndClearExpectations(&policy);

  // Clean up.
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

TEST_F(MemoryCoordinatorPolicyObserverTest, AddPolicyNotifiesExistingGroups) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(kChildId, &host);

  static constexpr char kConsumerId[] = "consumer";

  // Add a consumer group BEFORE adding the policy.
  policy_manager().OnConsumerGroupAdded(kConsumerId, kTestTraits1,
                                        PROCESS_TYPE_RENDERER, kChildId);

  MockObserverPolicy policy(policy_manager());

  // Adding the policy should trigger notification of the existing group.
  EXPECT_CALL(policy, OnConsumerGroupAdded(kConsumerId,
                                           std::make_optional(kTestTraits1),
                                           PROCESS_TYPE_RENDERER, kChildId));
  MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);
  Mock::VerifyAndClearExpectations(&policy);

  // Clean up.
  EXPECT_CALL(policy, OnConsumerGroupRemoved(kConsumerId, kChildId));
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

TEST_F(MemoryCoordinatorPolicyObserverTest, MultipleProcesses) {
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

  MockObserverPolicy policy(policy_manager());

  EXPECT_CALL(policy, OnConsumerGroupAdded(kConsumerId1, _, _, kChildId1));
  EXPECT_CALL(policy, OnConsumerGroupAdded(kConsumerId2, _, _, kChildId2));
  MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);

  // Update limit for both.
  EXPECT_CALL(host1, UpdateConsumers(ElementsAre(
                         MemoryConsumerUpdate{kConsumerId1, 50, false})));
  EXPECT_CALL(host2, UpdateConsumers(ElementsAre(
                         MemoryConsumerUpdate{kConsumerId2, 70, false})));
  policy.manager().UpdateConsumers(&policy,
                                   {{kChildId1, {kConsumerId1, 50, false}},
                                    {kChildId2, {kConsumerId2, 70, false}}});
  Mock::VerifyAndClearExpectations(&host1);
  Mock::VerifyAndClearExpectations(&host2);

  // Release memory for process 1 only.
  EXPECT_CALL(host1, UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                         kConsumerId1, std::nullopt, true})));
  EXPECT_CALL(host2, UpdateConsumers(_)).Times(0);
  policy.manager().UpdateConsumers(
      &policy, {{kChildId1, {kConsumerId1, std::nullopt, true}}});
  Mock::VerifyAndClearExpectations(&host1);
  Mock::VerifyAndClearExpectations(&host2);

  EXPECT_CALL(policy, OnConsumerGroupRemoved(kConsumerId1, kChildId1));
  policy_manager().OnConsumerGroupRemoved(kConsumerId1, kChildId1);

  EXPECT_CALL(policy, OnConsumerGroupRemoved(kConsumerId2, kChildId2));
  policy_manager().OnConsumerGroupRemoved(kConsumerId2, kChildId2);

  // Clean up.
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId1);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId2);
}

TEST_F(MemoryCoordinatorPolicyObserverTest, SameConsumerIdDifferentChild) {
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

  MockObserverPolicy policy(policy_manager());

  EXPECT_CALL(policy, OnConsumerGroupAdded(kConsumerId, _, _, kChildId1));
  EXPECT_CALL(policy, OnConsumerGroupAdded(kConsumerId, _, _, kChildId2));
  MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);

  // Each group can have its own limit even if they share the same ID.
  EXPECT_CALL(host1, UpdateConsumers(ElementsAre(
                         MemoryConsumerUpdate{kConsumerId, 40, false})));
  policy.manager().UpdateConsumers(&policy,
                                   {{kChildId1, {kConsumerId, 40, false}}});

  EXPECT_CALL(host2, UpdateConsumers(ElementsAre(
                         MemoryConsumerUpdate{kConsumerId, 60, false})));
  policy.manager().UpdateConsumers(&policy,
                                   {{kChildId2, {kConsumerId, 60, false}}});

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

TEST_F(MemoryCoordinatorPolicyObserverTest, MultipleConsumersSameChild) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(42);

  policy_manager().AddMemoryConsumerGroupHost(kChildId, &host);

  policy_manager().OnConsumerGroupAdded("consumer1", kTestTraits1,
                                        PROCESS_TYPE_RENDERER, kChildId);
  policy_manager().OnConsumerGroupAdded("consumer2", kTestTraits1,
                                        PROCESS_TYPE_RENDERER, kChildId);

  MockObserverPolicy policy(policy_manager());

  EXPECT_CALL(policy, OnConsumerGroupAdded("consumer1", _, _, kChildId));
  EXPECT_CALL(policy, OnConsumerGroupAdded("consumer2", _, _, kChildId));
  MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);

  EXPECT_CALL(host, UpdateConsumers(ElementsAre(
                        MemoryConsumerUpdate{"consumer1", 50, false})));
  EXPECT_CALL(host, UpdateConsumers(ElementsAre(
                        MemoryConsumerUpdate{"consumer2", 80, false})));
  policy.manager().UpdateConsumers(&policy,
                                   {{kChildId, {"consumer1", 50, false}}});
  policy.manager().UpdateConsumers(&policy,
                                   {{kChildId, {"consumer2", 80, false}}});

  EXPECT_CALL(policy, OnConsumerGroupRemoved("consumer1", kChildId));
  policy_manager().OnConsumerGroupRemoved("consumer1", kChildId);

  EXPECT_CALL(policy, OnConsumerGroupRemoved("consumer2", kChildId));
  policy_manager().OnConsumerGroupRemoved("consumer2", kChildId);

  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
TEST_F(MemoryCoordinatorPolicyManagerTest,
       AddDiagnosticObserverNotifiesExistingLimits) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(42);
  static constexpr char kConsumerId[] = "consumer";

  policy_manager().AddMemoryConsumerGroupHost(kChildId, &host);
  policy_manager().OnConsumerGroupAdded(kConsumerId, kTestTraits1,
                                        PROCESS_TYPE_RENDERER, kChildId);

  MockPolicy policy(policy_manager());
  MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);

  // Set an initial limit.
  EXPECT_CALL(host, UpdateConsumers(ElementsAre(
                        MemoryConsumerUpdate{kConsumerId, 50, false})));
  policy.manager().UpdateConsumers(&policy,
                                   {{kChildId, {kConsumerId, 50, false}}});
  Mock::VerifyAndClearExpectations(&host);

  // Adding a diagnostic observer should immediately notify the current limit.
  MockDiagnosticObserver observer;
  EXPECT_CALL(observer, OnMemoryLimitChanged(kConsumerId, kChildId, 50));
  policy_manager().AddDiagnosticObserver(&observer);
  Mock::VerifyAndClearExpectations(&observer);

  // Clean up.
  policy_manager().RemoveDiagnosticObserver(&observer);
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}
#endif

}  // namespace content
