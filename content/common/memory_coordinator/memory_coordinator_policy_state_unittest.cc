// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/memory_coordinator/memory_coordinator_policy_state.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "base/hash/hash.h"
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

class MockMemoryConsumerGroupHost : public MemoryConsumerGroupHost {
 public:
  MOCK_METHOD(void,
              UpdateConsumers,
              (std::vector<MemoryConsumerUpdate> updates),
              (override));
};

class TestPolicy : public MemoryCoordinatorPolicy {
 public:
  TestPolicy(MemoryCoordinatorPolicyManager& manager,
             MemoryCoordinatorPolicyState::ConsumerPredicate predicate)
      : MemoryCoordinatorPolicy(manager),
        state_(*this, manager, std::move(predicate)) {}

  MemoryCoordinatorPolicyState& state() { return state_; }

 private:
  MemoryCoordinatorPolicyState state_;
};

}  // namespace

class MemoryCoordinatorPolicyStateTest : public testing::Test {
 protected:
  MemoryCoordinatorPolicyManager& policy_manager() { return policy_manager_; }

 private:
  base::test::TaskEnvironment task_environment_;
  MemoryCoordinatorPolicyManager policy_manager_;
};

TEST_F(MemoryCoordinatorPolicyStateTest, Persistence) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId;
  const ChildProcessId kOtherChildId(1);

  policy_manager().AddMemoryConsumerGroupHost(kChildId, &host);
  policy_manager().AddMemoryConsumerGroupHost(kOtherChildId, &host);

  TestPolicy policy(
      policy_manager(),
      base::BindRepeating([](uint32_t consumer_id,
                             std::optional<base::MemoryConsumerTraits> traits,
                             ProcessType process_type,
                             ChildProcessId child_process_id) {
        // Only match consumers in the primary process (kChildId).
        return child_process_id.is_null();
      }));

  policy.state().SetLimit(50, true);

  // A consumer added AFTER the limit was set should immediately receive it if
  // it matches the predicate.
  const std::string kConsumerName1 = "consumer1";
  const uint32_t kConsumerId1 = base::PersistentHash(kConsumerName1);

  EXPECT_CALL(host, UpdateConsumers(UnorderedElementsAre(
                        MemoryConsumerUpdate{kConsumerId1, 50, true})));
  policy_manager().OnConsumerGroupAdded(kConsumerId1, kConsumerName1, {},
                                        PROCESS_TYPE_BROWSER, kChildId);
  Mock::VerifyAndClearExpectations(&host);

  // A consumer added AFTER the limit was set should NOT receive it if it
  // doesn't match the predicate.
  const std::string kConsumerName2 = "consumer2";
  const uint32_t kConsumerId2 = base::PersistentHash(kConsumerName2);

  EXPECT_CALL(host, UpdateConsumers(_)).Times(0);
  policy_manager().OnConsumerGroupAdded(kConsumerId2, kConsumerName2, {},
                                        PROCESS_TYPE_BROWSER, kOtherChildId);
  Mock::VerifyAndClearExpectations(&host);

  policy_manager().OnConsumerGroupRemoved(kConsumerId1, kChildId);
  policy_manager().OnConsumerGroupRemoved(kConsumerId2, kOtherChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kOtherChildId);
}

TEST_F(MemoryCoordinatorPolicyStateTest, SetLimit) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId;

  policy_manager().AddMemoryConsumerGroupHost(kChildId, &host);

  const std::string kConsumerName1 = "consumer1";
  const uint32_t kConsumerId1 = base::PersistentHash(kConsumerName1);
  const std::string kConsumerName2 = "consumer2";
  const uint32_t kConsumerId2 = base::PersistentHash(kConsumerName2);

  policy_manager().OnConsumerGroupAdded(kConsumerId1, kConsumerName1, {},
                                        PROCESS_TYPE_BROWSER, kChildId);
  policy_manager().OnConsumerGroupAdded(kConsumerId2, kConsumerName2, {},
                                        PROCESS_TYPE_BROWSER, kChildId);

  TestPolicy policy(
      policy_manager(),
      base::BindRepeating([](uint32_t consumer_id,
                             std::optional<base::MemoryConsumerTraits> traits,
                             ProcessType process_type,
                             ChildProcessId child_process_id) {
        return child_process_id.is_null();
      }));

  // Updating the limit should update all matching existing consumers.
  EXPECT_CALL(host, UpdateConsumers(UnorderedElementsAre(
                        MemoryConsumerUpdate{kConsumerId1, 50, true},
                        MemoryConsumerUpdate{kConsumerId2, 50, true})));
  policy.state().SetLimit(50, true);
  Mock::VerifyAndClearExpectations(&host);

  // Resetting the limit should update all matching existing consumers.
  EXPECT_CALL(host, UpdateConsumers(UnorderedElementsAre(
                        MemoryConsumerUpdate{kConsumerId1, 100, false},
                        MemoryConsumerUpdate{kConsumerId2, 100, false})));
  policy.state().SetLimit(100, false);
  Mock::VerifyAndClearExpectations(&host);

  policy_manager().OnConsumerGroupRemoved(kConsumerId1, kChildId);
  policy_manager().OnConsumerGroupRemoved(kConsumerId2, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

TEST_F(MemoryCoordinatorPolicyStateTest, ChangeReleaseMemory) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId;

  policy_manager().AddMemoryConsumerGroupHost(kChildId, &host);

  const std::string kConsumerName = "consumer1";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName, {},
                                        PROCESS_TYPE_BROWSER, kChildId);

  TestPolicy policy(
      policy_manager(),
      base::BindRepeating([](uint32_t consumer_id,
                             std::optional<base::MemoryConsumerTraits> traits,
                             ProcessType process_type,
                             ChildProcessId child_process_id) {
        return child_process_id.is_null();
      }));

  // Initial set limit.
  EXPECT_CALL(host, UpdateConsumers(UnorderedElementsAre(
                        MemoryConsumerUpdate{kConsumerId, 50, false})));
  policy.state().SetLimit(50, false);
  Mock::VerifyAndClearExpectations(&host);

  // If the limit is the same but release_memory changes, the limit update
  // should be std::nullopt.
  EXPECT_CALL(host, UpdateConsumers(UnorderedElementsAre(MemoryConsumerUpdate{
                        kConsumerId, std::nullopt, true})));
  policy.state().SetLimit(50, true);
  Mock::VerifyAndClearExpectations(&host);

  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

TEST_F(MemoryCoordinatorPolicyStateTest, DefaultStatePersistence) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId;

  policy_manager().AddMemoryConsumerGroupHost(kChildId, &host);

  TestPolicy policy(
      policy_manager(),
      base::BindRepeating([](uint32_t consumer_id,
                             std::optional<base::MemoryConsumerTraits> traits,
                             ProcessType process_type,
                             ChildProcessId child_process_id) {
        return child_process_id.is_null();
      }));

  // Adding a consumer when the policy is in its default state (100% limit,
  // no release) should NOT trigger an update.
  const std::string kConsumerName = "consumer1";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  EXPECT_CALL(host, UpdateConsumers(_)).Times(0);
  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName, {},
                                        PROCESS_TYPE_BROWSER, kChildId);
  Mock::VerifyAndClearExpectations(&host);

  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

TEST_F(MemoryCoordinatorPolicyStateTest, ObserverLifecycle) {
  MemoryCoordinatorPolicyState::ConsumerPredicate predicate =
      base::BindRepeating([](uint32_t consumer_id,
                             std::optional<base::MemoryConsumerTraits> traits,
                             ProcessType process_type,
                             ChildProcessId child_process_id) { return true; });

  // When a consumer is added, it should only be notified if the policy state
  // hasn't been destroyed.
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId;
  policy_manager().AddMemoryConsumerGroupHost(kChildId, &host);

  const std::string kConsumerName = "consumer1";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  {
    TestPolicy policy(policy_manager(), predicate);
    policy.state().SetLimit(50, false);

    EXPECT_CALL(host, UpdateConsumers(_)).Times(1);
    policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName, {},
                                          PROCESS_TYPE_BROWSER, kChildId);
    Mock::VerifyAndClearExpectations(&host);
    policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  }

  // After destruction, it should no longer be an observer.
  EXPECT_CALL(host, UpdateConsumers(_)).Times(0);
  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName, {},
                                        PROCESS_TYPE_BROWSER, kChildId);
  Mock::VerifyAndClearExpectations(&host);

  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

TEST_F(MemoryCoordinatorPolicyStateTest, RepeatedReleaseForStatelessConsumers) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId;

  policy_manager().AddMemoryConsumerGroupHost(kChildId, &host);

  // Define traits for stateful and stateless consumers.
  constexpr base::MemoryConsumerTraits kStatefulTraits(
      base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
      base::MemoryConsumerTraits::ReleaseMemoryCost::kRequiresTraversal,
      base::MemoryConsumerTraits::InformationRetention::kLossless,
      base::MemoryConsumerTraits::ExecutionType::
          kSynchronous);  // Default is stateful (kYes)

  constexpr base::MemoryConsumerTraits kStatelessTraits(
      base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
      base::MemoryConsumerTraits::ReleaseMemoryCost::kRequiresTraversal,
      base::MemoryConsumerTraits::InformationRetention::kLossless,
      base::MemoryConsumerTraits::ExecutionType::kSynchronous,
      base::MemoryConsumerTraits::IsStateful::kNo);

  const std::string kStatefulName = "stateful";
  const uint32_t kStatefulId = base::PersistentHash(kStatefulName);
  const std::string kStatelessName = "stateless";
  const uint32_t kStatelessId = base::PersistentHash(kStatelessName);
  const std::string kNoTraitsName = "no_traits";
  const uint32_t kNoTraitsId = base::PersistentHash(kNoTraitsName);

  policy_manager().OnConsumerGroupAdded(kStatefulId, kStatefulName,
                                        kStatefulTraits, PROCESS_TYPE_BROWSER,
                                        kChildId);
  policy_manager().OnConsumerGroupAdded(kStatelessId, kStatelessName,
                                        kStatelessTraits, PROCESS_TYPE_BROWSER,
                                        kChildId);
  policy_manager().OnConsumerGroupAdded(
      kNoTraitsId, kNoTraitsName, std::nullopt, PROCESS_TYPE_BROWSER, kChildId);

  TestPolicy policy(
      policy_manager(),
      base::BindRepeating([](uint32_t consumer_id,
                             std::optional<base::MemoryConsumerTraits> traits,
                             ProcessType process_type,
                             ChildProcessId child_process_id) {
        return child_process_id.is_null();
      }));

  // Critical pressure (limit 0, release true): all should be notified
  // initially.
  EXPECT_CALL(host, UpdateConsumers(UnorderedElementsAre(
                        MemoryConsumerUpdate{kStatefulId, 0, true},
                        MemoryConsumerUpdate{kStatelessId, 0, true},
                        MemoryConsumerUpdate{kNoTraitsId, 0, true})));
  policy.state().SetLimit(0, true);
  Mock::VerifyAndClearExpectations(&host);

  // Simulate repeated critical pressure: stateless and no_traits consumers
  // notified. Stateful is skipped.
  EXPECT_CALL(host,
              UpdateConsumers(UnorderedElementsAre(
                  MemoryConsumerUpdate{kStatelessId, std::nullopt, true},
                  MemoryConsumerUpdate{kNoTraitsId, std::nullopt, true})));
  policy.state().SetLimit(0, true);
  Mock::VerifyAndClearExpectations(&host);

  // Transition to Moderate pressure (limit 50, release true): all should be
  // notified.
  EXPECT_CALL(host, UpdateConsumers(UnorderedElementsAre(
                        MemoryConsumerUpdate{kStatefulId, 50, true},
                        MemoryConsumerUpdate{kStatelessId, 50, true},
                        MemoryConsumerUpdate{kNoTraitsId, 50, true})));
  policy.state().SetLimit(50, true);
  Mock::VerifyAndClearExpectations(&host);

  // Simulate repeated moderate pressure: stateless and no_traits consumers
  // notified. Stateful is skipped.
  EXPECT_CALL(host,
              UpdateConsumers(UnorderedElementsAre(
                  MemoryConsumerUpdate{kStatelessId, std::nullopt, true},
                  MemoryConsumerUpdate{kNoTraitsId, std::nullopt, true})));
  policy.state().SetLimit(50, true);
  Mock::VerifyAndClearExpectations(&host);

  // Stop pressure (limit 100, release true): all should be reset to 100%.
  EXPECT_CALL(host, UpdateConsumers(UnorderedElementsAre(
                        MemoryConsumerUpdate{kStatefulId, 100, true},
                        MemoryConsumerUpdate{kStatelessId, 100, true},
                        MemoryConsumerUpdate{kNoTraitsId, 100, true})));
  policy.state().SetLimit(100, true);
  Mock::VerifyAndClearExpectations(&host);

  // Simulate repeated no pressure: should NOT notify because limit is 100 (not
  // under pressure).
  EXPECT_CALL(host, UpdateConsumers(_)).Times(0);
  policy.state().SetLimit(100, true);
  Mock::VerifyAndClearExpectations(&host);

  policy_manager().OnConsumerGroupRemoved(kStatefulId, kChildId);
  policy_manager().OnConsumerGroupRemoved(kStatelessId, kChildId);
  policy_manager().OnConsumerGroupRemoved(kNoTraitsId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

}  // namespace content
