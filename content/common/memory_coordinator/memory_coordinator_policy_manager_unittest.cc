// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"

#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/memory_coordinator/memory_coordinator_features.h"
#include "base/memory_coordinator/traits.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "content/common/buildflags.h"
#include "content/common/memory_coordinator/memory_consumer_group_host.h"
#include "content/public/common/memory_coordinator/memory_coordinator_policy.h"
#include "content/public/test/memory_coordinator_browsertest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Test;

constexpr base::MemoryConsumerTraits kTestTraits1(
    base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
    base::MemoryConsumerTraits::ReleaseMemoryDataAccess::kNone,
    base::MemoryConsumerTraits::InformationRetention::kLossy,
    base::MemoryConsumerTraits::ExecutionType::kSynchronous);

constexpr base::MemoryConsumerTraits kStatelessTraits(
    base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
    base::MemoryConsumerTraits::ReleaseMemoryDataAccess::kNone,
    base::MemoryConsumerTraits::InformationRetention::kLossy,
    base::MemoryConsumerTraits::ExecutionType::kSynchronous,
    base::MemoryConsumerTraits::IsStateful::kNo);

class MockPolicy : public MemoryCoordinatorPolicy {
 public:
  explicit MockPolicy(MemoryCoordinatorPolicyManager& manager)
      : MemoryCoordinatorPolicy(manager) {}

  // MemoryCoordinatorPolicy:
  void OnConsumerGroupAdded(uint32_t consumer_id,
                            std::string_view consumer_name,
                            base::MemoryConsumerTraits traits,
                            ProcessType process_type,
                            ChildProcessId child_process_id) override {}
  void OnConsumerGroupRemoved(uint32_t consumer_id,
                              ChildProcessId child_process_id) override {}

  using MemoryCoordinatorPolicy::manager;
};

class MockMemoryConsumerGroupHost : public MemoryConsumerGroupHost {
 public:
  MOCK_METHOD(void,
              UpdateConsumers,
              (std::vector<MemoryConsumerUpdate> updates),
              (override));
  MOCK_METHOD(void,
              SetOverrideLimit,
              (uint32_t consumer_id, base::MemoryLimit memory_limit),
              (override));
  MOCK_METHOD(void,
              ClearOverrideLimit,
              (uint32_t consumer_id, base::MemoryLimit policy_limit),
              (override));
};

// Returns a filter that matches the consumer group with `target_id`.
MemoryCoordinatorPolicyManager::ConsumerFilter MatchConsumerId(
    uint32_t target_id) {
  return base::BindRepeating(
      [](uint32_t target_id, uint32_t consumer_id,
         std::string_view consumer_name, base::MemoryConsumerTraits traits,
         ProcessType process_type,
         ChildProcessId child_process_id) { return consumer_id == target_id; },
      target_id);
}

}  // namespace

class MemoryCoordinatorPolicyManagerTest : public Test {
 protected:
  MemoryCoordinatorPolicyManager& policy_manager() { return policy_manager_; }

 private:
  // Whether a dropped release request is replayed depends on whether the group
  // is stateful (see IsEffectivelyStateful()). Enable stateful memory pressure
  // so that it doesn't depend on the platform's default.
  base::test::ScopedFeatureList scoped_feature_list_{
      base::kStatefulMemoryPressure};
  base::test::TaskEnvironment task_environment_;
  MemoryCoordinatorPolicyManager policy_manager_;
};

TEST_F(MemoryCoordinatorPolicyManagerTest, AggregateMemoryLimit) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  NiceMock<MockPolicy> policy1(policy_manager());
  MemoryCoordinatorPolicyRegistration reg1(policy_manager(), policy1);

  NiceMock<MockPolicy> policy2(policy_manager());
  MemoryCoordinatorPolicyRegistration reg2(policy_manager(), policy2);

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kChildId);

  // Both policies request a limit. They should be multiplied together.
  // Initial limit is 100%. Changes to 80%.
  EXPECT_CALL(host,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId, base::MemoryLimit::FromPercent(80), false})));
  policy1.manager().UpdateConsumers(
      &policy1,
      {{kChildId, {kConsumerId, base::MemoryLimit::FromPercent(80), false}}});
  Mock::VerifyAndClearExpectations(&host);

  // policy1=80%, policy2=50% -> 40%.
  EXPECT_CALL(host,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId, base::MemoryLimit::FromPercent(40), false})));
  policy2.manager().UpdateConsumers(
      &policy2,
      {{kChildId, {kConsumerId, base::MemoryLimit::FromPercent(50), false}}});
  Mock::VerifyAndClearExpectations(&host);

  // policy1=90%, policy2=50% -> 45%.
  EXPECT_CALL(host,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId, base::MemoryLimit::FromPercent(45), false})));
  policy1.manager().UpdateConsumers(
      &policy1,
      {{kChildId, {kConsumerId, base::MemoryLimit::FromPercent(90), false}}});
  Mock::VerifyAndClearExpectations(&host);

  // policy1=90%, policy2=100% -> 90%.
  EXPECT_CALL(host,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId, base::MemoryLimit::FromPercent(90), false})));
  policy2.manager().UpdateConsumers(
      &policy2,
      {{kChildId, {kConsumerId, base::MemoryLimit::FromPercent(100), false}}});
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

TEST_F(MemoryCoordinatorPolicyManagerTest, AggregateMemoryLimit_Boosting) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  NiceMock<MockPolicy> policy1(policy_manager());
  MemoryCoordinatorPolicyRegistration reg1(policy_manager(), policy1);

  NiceMock<MockPolicy> policy2(policy_manager());
  MemoryCoordinatorPolicyRegistration reg2(policy_manager(), policy2);

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kChildId);

  // policy1 requests 150%. Changes from 100% to 150%.
  EXPECT_CALL(host,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId, base::MemoryLimit::FromPercent(150), false})));
  policy1.manager().UpdateConsumers(
      &policy1,
      {{kChildId, {kConsumerId, base::MemoryLimit::FromPercent(150), false}}});
  Mock::VerifyAndClearExpectations(&host);

  // policy2 requests 80%. Multiplied together (150% * 80%) = 120%.
  EXPECT_CALL(host,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId, base::MemoryLimit::FromPercent(120), false})));
  policy2.manager().UpdateConsumers(
      &policy2,
      {{kChildId, {kConsumerId, base::MemoryLimit::FromPercent(80), false}}});
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

TEST_F(MemoryCoordinatorPolicyManagerTest, RemovePolicyClearsData) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  NiceMock<MockPolicy> policy1(policy_manager());
  NiceMock<MockPolicy> policy2(policy_manager());
  MemoryCoordinatorPolicyRegistration reg2(policy_manager(), policy2);

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kChildId);

  {
    MemoryCoordinatorPolicyRegistration reg1(policy_manager(), policy1);

    // policy1 requests 50%. Changes from 100% to 50%.
    EXPECT_CALL(host,
                UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                    kConsumerId, base::MemoryLimit::FromPercent(50), false})));
    policy1.manager().UpdateConsumers(
        &policy1,
        {{kChildId, {kConsumerId, base::MemoryLimit::FromPercent(50), false}}});
    Mock::VerifyAndClearExpectations(&host);

    // policy2 requests 80%. Multiplied together (50% * 80%) = 40%.
    EXPECT_CALL(host,
                UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                    kConsumerId, base::MemoryLimit::FromPercent(40), false})));
    policy2.manager().UpdateConsumers(
        &policy2,
        {{kChildId, {kConsumerId, base::MemoryLimit::FromPercent(80), false}}});
    Mock::VerifyAndClearExpectations(&host);

    // Removing policy1 should clear its 50% request, so the limit should become
    // 80% (from policy2). Changes from 40% to 80%.
    EXPECT_CALL(host,
                UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                    kConsumerId, base::MemoryLimit::FromPercent(80), false})));
  }
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

TEST_F(MemoryCoordinatorPolicyManagerTest, ReleaseMemory) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  NiceMock<MockPolicy> policy(policy_manager());
  MemoryCoordinatorPolicyRegistration reg(policy_manager(), policy);

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kChildId);

  EXPECT_CALL(host, UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                        kConsumerId, std::nullopt, true})));
  policy.manager().UpdateConsumers(
      &policy, {{kChildId, {kConsumerId, std::nullopt, true}}});
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

TEST_F(MemoryCoordinatorPolicyManagerTest,
       PassiveConsumerDoesNotReceiveReleaseMemory) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  NiceMock<MockPolicy> policy(policy_manager());
  MemoryCoordinatorPolicyRegistration reg(policy_manager(), policy);

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  constexpr base::MemoryConsumerTraits kPassiveTraits(
      base::MemoryConsumerTraits::ConsumerType::kPassive);
  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kPassiveTraits, kChildId);

  // A release-only request is dropped entirely.
  EXPECT_CALL(host, UpdateConsumers(_)).Times(0);
  policy.manager().UpdateConsumers(
      &policy, {{kChildId, {kConsumerId, std::nullopt, true}}});
  Mock::VerifyAndClearExpectations(&host);

  // A limit change with a release request only delivers the limit.
  EXPECT_CALL(host,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId, base::MemoryLimit::FromPercent(50), false})));
  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId, {kConsumerId, base::MemoryLimit::FromPercent(50), true}}});
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

TEST_F(MemoryCoordinatorPolicyManagerTest, SetMemoryLimitOverride) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kChildId);

  // Set override.
  EXPECT_CALL(
      host, SetOverrideLimit(kConsumerId, base::MemoryLimit::FromPercent(42)));
  policy_manager().SetMemoryLimitOverride(kConsumerId,
                                          base::MemoryLimit::FromPercent(42));
  Mock::VerifyAndClearExpectations(&host);

  // Update override.
  EXPECT_CALL(
      host, SetOverrideLimit(kConsumerId, base::MemoryLimit::FromPercent(24)));
  policy_manager().SetMemoryLimitOverride(kConsumerId,
                                          base::MemoryLimit::FromPercent(24));
  Mock::VerifyAndClearExpectations(&host);

  // Clear override. Reverts to default (100%).
  EXPECT_CALL(host,
              ClearOverrideLimit(kConsumerId, base::MemoryLimit::Default()));
  policy_manager().ClearMemoryLimitOverride(kConsumerId);
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

TEST_F(MemoryCoordinatorPolicyManagerTest, SetMemoryLimitOverride_Persistence) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  // Set override BEFORE adding consumer.
  // Since host is already added, it should receive the IPC immediately.
  EXPECT_CALL(
      host, SetOverrideLimit(kConsumerId, base::MemoryLimit::FromPercent(42)));
  policy_manager().SetMemoryLimitOverride(kConsumerId,
                                          base::MemoryLimit::FromPercent(42));
  Mock::VerifyAndClearExpectations(&host);

  // Adding consumer to an out-of-process host should NOT send a duplicate
  // override IPC since the host was already notified.
  EXPECT_CALL(host, SetOverrideLimit(_, _)).Times(0);
  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kChildId);
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  EXPECT_CALL(host,
              ClearOverrideLimit(kConsumerId, base::MemoryLimit::Default()));
  policy_manager().ClearMemoryLimitOverride(kConsumerId);
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

TEST_F(MemoryCoordinatorPolicyManagerTest,
       SetMemoryLimitOverride_HostPersistence) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  // Set override BEFORE adding host.
  policy_manager().SetMemoryLimitOverride(kConsumerId,
                                          base::MemoryLimit::FromPercent(42));

  // Adding host should immediately send the override.
  EXPECT_CALL(
      host, SetOverrideLimit(kConsumerId, base::MemoryLimit::FromPercent(42)));
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);
  Mock::VerifyAndClearExpectations(&host);

  // Adding consumer to an out-of-process host should NOT send a duplicate
  // override IPC since the host was already notified.
  EXPECT_CALL(host, SetOverrideLimit(_, _)).Times(0);
  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kChildId);
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  EXPECT_CALL(host,
              ClearOverrideLimit(kConsumerId, base::MemoryLimit::Default()));
  policy_manager().ClearMemoryLimitOverride(kConsumerId);
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

TEST_F(MemoryCoordinatorPolicyManagerTest,
       SetMemoryLimitOverride_InProcessPersistence) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kInProcessChildId;

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  // Set override BEFORE adding host.
  policy_manager().SetMemoryLimitOverride(kConsumerId,
                                          base::MemoryLimit::FromPercent(42));

  // Adding in-process host.
  EXPECT_CALL(
      host, SetOverrideLimit(kConsumerId, base::MemoryLimit::FromPercent(42)));
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_BROWSER,
                                              kInProcessChildId, &host);
  Mock::VerifyAndClearExpectations(&host);

  // Adding consumer to an in-process host SHOULD notify the host so that
  // in-process registries (which do not buffer overrides) apply the limit.
  EXPECT_CALL(
      host, SetOverrideLimit(kConsumerId, base::MemoryLimit::FromPercent(42)));
  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kInProcessChildId);
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  EXPECT_CALL(host,
              ClearOverrideLimit(kConsumerId, base::MemoryLimit::Default()));
  policy_manager().ClearMemoryLimitOverride(kConsumerId);
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kInProcessChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kInProcessChildId);
}

TEST_F(MemoryCoordinatorPolicyManagerTest, NotifyReleaseMemory) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kChildId);

  EXPECT_CALL(host, UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                        kConsumerId, std::nullopt, true})));
  policy_manager().NotifyReleaseMemoryForTesting(kConsumerId);
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

TEST_F(MemoryCoordinatorPolicyManagerTest, UpdateConsumers_MultipleProcesses) {
  MockMemoryConsumerGroupHost host1;
  MockMemoryConsumerGroupHost host2;
  const ChildProcessId kChildId1(1);
  const ChildProcessId kChildId2(2);
  const char kConsumerName1[] = "consumer1";
  const uint32_t kConsumerId1 = base::PersistentHash(kConsumerName1);
  const char kConsumerName2[] = "consumer2";
  const uint32_t kConsumerId2 = base::PersistentHash(kConsumerName2);

  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId1,
                                              &host1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId2,
                                              &host2);

  policy_manager().OnConsumerGroupAdded(kConsumerId1, kConsumerName1,
                                        kTestTraits1, kChildId1);
  policy_manager().OnConsumerGroupAdded(kConsumerId2, kConsumerName2,
                                        kTestTraits1, kChildId2);

  MockPolicy policy(policy_manager());
  MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);

  EXPECT_CALL(host1,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId1, base::MemoryLimit::FromPercent(50), true})));
  EXPECT_CALL(host2,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId2, base::MemoryLimit::FromPercent(80), false})));

  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId1, {kConsumerId1, base::MemoryLimit::FromPercent(50), true}},
       {kChildId2, {kConsumerId2, base::MemoryLimit::FromPercent(80), false}}});

  Mock::VerifyAndClearExpectations(&host1);
  Mock::VerifyAndClearExpectations(&host2);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId1, kChildId1);
  policy_manager().OnConsumerGroupRemoved(kConsumerId2, kChildId2);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId1);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId2);
}

TEST_F(MemoryCoordinatorPolicyManagerTest, UpdateConsumers_Filter) {
  MockMemoryConsumerGroupHost host1;
  MockMemoryConsumerGroupHost host2;
  const ChildProcessId kChildId1(1);
  const ChildProcessId kChildId2(2);
  const char kConsumerName1[] = "consumer1";
  const uint32_t kConsumerId1 = base::PersistentHash(kConsumerName1);
  const char kConsumerName2[] = "consumer2";
  const uint32_t kConsumerId2 = base::PersistentHash(kConsumerName2);

  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId1,
                                              &host1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId2,
                                              &host2);

  // 2 test traits that differ by their SupportsMemoryLimit value.
  const base::MemoryConsumerTraits kTraits1(
      base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
      base::MemoryConsumerTraits::ReleaseMemoryDataAccess::kReads,
      base::MemoryConsumerTraits::InformationRetention::kLossy,
      base::MemoryConsumerTraits::ExecutionType::kSynchronous,
      base::MemoryConsumerTraits::SupportsMemoryLimit::kYes);
  const base::MemoryConsumerTraits kTraits2(
      base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
      base::MemoryConsumerTraits::ReleaseMemoryDataAccess::kReads,
      base::MemoryConsumerTraits::InformationRetention::kLossy,
      base::MemoryConsumerTraits::ExecutionType::kSynchronous,
      base::MemoryConsumerTraits::SupportsMemoryLimit::kNo);

  policy_manager().OnConsumerGroupAdded(kConsumerId1, kConsumerName1, kTraits1,
                                        kChildId1);
  policy_manager().OnConsumerGroupAdded(kConsumerId2, kConsumerName2, kTraits2,
                                        kChildId2);

  MockPolicy policy(policy_manager());
  MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);

  // Update only consumers with kTraits1.
  EXPECT_CALL(host1,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId1, base::MemoryLimit::FromPercent(50), true})));
  EXPECT_CALL(host2, UpdateConsumers(_)).Times(0);

  policy.manager().UpdateConsumers(
      &policy,
      [](uint32_t consumer_id, std::string_view consumer_name,
         base::MemoryConsumerTraits traits, ProcessType process_type,
         ChildProcessId child_process_id) {
        return traits.supports_memory_limit ==
               base::MemoryConsumerTraits::SupportsMemoryLimit::kYes;
      },
      base::MemoryLimit::FromPercent(50), true);

  Mock::VerifyAndClearExpectations(&host1);
  Mock::VerifyAndClearExpectations(&host2);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId1, kChildId1);
  policy_manager().OnConsumerGroupRemoved(kConsumerId2, kChildId2);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId1);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId2);
}

namespace {

class MockObserverPolicy : public MemoryCoordinatorPolicy {
 public:
  explicit MockObserverPolicy(MemoryCoordinatorPolicyManager& manager)
      : MemoryCoordinatorPolicy(manager) {}

  MOCK_METHOD(void,
              OnConsumerGroupAdded,
              (uint32_t consumer_id,
               std::string_view consumer_name,
               base::MemoryConsumerTraits traits,
               ProcessType process_type,
               ChildProcessId child_process_id),
              (override));
  MOCK_METHOD(void,
              OnConsumerGroupRemoved,
              (uint32_t consumer_id, ChildProcessId child_process_id),
              (override));

  using MemoryCoordinatorPolicy::manager;
};

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
class MockDiagnosticObserver
    : public MemoryCoordinatorPolicyManager::DiagnosticObserver {
 public:
  MOCK_METHOD(void,
              OnMemoryLimitChanged,
              (uint32_t consumer_id,
               ChildProcessId child_process_id,
               base::MemoryLimit memory_limit),
              (override));
};
#endif

}  // namespace

class MemoryCoordinatorPolicyObserverTest
    : public MemoryCoordinatorPolicyManagerTest {};

TEST_F(MemoryCoordinatorPolicyObserverTest, PolicyNotification) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  MockObserverPolicy policy(policy_manager());
  MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  EXPECT_CALL(policy,
              OnConsumerGroupAdded(kConsumerId, kConsumerName, kTestTraits1,
                                   PROCESS_TYPE_RENDERER, kChildId));
  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kChildId);
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
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  // Add a consumer group BEFORE adding the policy.
  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kChildId);

  MockObserverPolicy policy(policy_manager());

  // Adding the policy should trigger notification of the existing group.
  EXPECT_CALL(policy,
              OnConsumerGroupAdded(kConsumerId, kConsumerName, kTestTraits1,
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

  const char kConsumerName1[] = "consumer1";
  const uint32_t kConsumerId1 = base::PersistentHash(kConsumerName1);
  const char kConsumerName2[] = "consumer2";
  const uint32_t kConsumerId2 = base::PersistentHash(kConsumerName2);
  const ChildProcessId kChildId1(1);
  const ChildProcessId kChildId2(2);

  // Register hosts.
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId1,
                                              &host1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId2,
                                              &host2);

  // Process 1 adds "consumer1"
  policy_manager().OnConsumerGroupAdded(kConsumerId1, kConsumerName1,
                                        kTestTraits1, kChildId1);

  // Process 2 adds "consumer2"
  policy_manager().OnConsumerGroupAdded(kConsumerId2, kConsumerName2,
                                        kTestTraits1, kChildId2);

  MockObserverPolicy policy(policy_manager());

  EXPECT_CALL(policy, OnConsumerGroupAdded(kConsumerId1, kConsumerName1, _, _,
                                           kChildId1));
  EXPECT_CALL(policy, OnConsumerGroupAdded(kConsumerId2, kConsumerName2, _, _,
                                           kChildId2));
  MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);

  // Update limit for both.
  EXPECT_CALL(host1,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId1, base::MemoryLimit::FromPercent(50), false})));
  EXPECT_CALL(host2,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId2, base::MemoryLimit::FromPercent(70), false})));
  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId1, {kConsumerId1, base::MemoryLimit::FromPercent(50), false}},
       {kChildId2, {kConsumerId2, base::MemoryLimit::FromPercent(70), false}}});
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

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);
  const ChildProcessId kChildId1(1);
  const ChildProcessId kChildId2(2);

  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId1,
                                              &host1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId2,
                                              &host2);

  // Multiple processes have the same consumer ID.
  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kChildId1);
  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kChildId2);

  MockObserverPolicy policy(policy_manager());

  EXPECT_CALL(policy, OnConsumerGroupAdded(kConsumerId, kConsumerName, _, _,
                                           kChildId1));
  EXPECT_CALL(policy, OnConsumerGroupAdded(kConsumerId, kConsumerName, _, _,
                                           kChildId2));
  MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);

  // Each group can have its own limit even if they share the same ID.
  EXPECT_CALL(host1,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId, base::MemoryLimit::FromPercent(40), false})));
  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId1, {kConsumerId, base::MemoryLimit::FromPercent(40), false}}});

  EXPECT_CALL(host2,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId, base::MemoryLimit::FromPercent(60), false})));
  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId2, {kConsumerId, base::MemoryLimit::FromPercent(60), false}}});

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
  const char kConsumerName1[] = "consumer1";
  const uint32_t kConsumerId1 = base::PersistentHash(kConsumerName1);
  const char kConsumerName2[] = "consumer2";
  const uint32_t kConsumerId2 = base::PersistentHash(kConsumerName2);

  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  policy_manager().OnConsumerGroupAdded(kConsumerId1, kConsumerName1,
                                        kTestTraits1, kChildId);
  policy_manager().OnConsumerGroupAdded(kConsumerId2, kConsumerName2,
                                        kTestTraits1, kChildId);

  MockObserverPolicy policy(policy_manager());

  EXPECT_CALL(policy, OnConsumerGroupAdded(kConsumerId1, kConsumerName1, _, _,
                                           kChildId));
  EXPECT_CALL(policy, OnConsumerGroupAdded(kConsumerId2, kConsumerName2, _, _,
                                           kChildId));
  MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);

  ::testing::InSequence seq;
  EXPECT_CALL(host,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId1, base::MemoryLimit::FromPercent(50), false})));
  EXPECT_CALL(host,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId2, base::MemoryLimit::FromPercent(80), false})));
  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId, {kConsumerId1, base::MemoryLimit::FromPercent(50), false}}});
  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId, {kConsumerId2, base::MemoryLimit::FromPercent(80), false}}});

  EXPECT_CALL(policy, OnConsumerGroupRemoved(kConsumerId1, kChildId));
  policy_manager().OnConsumerGroupRemoved(kConsumerId1, kChildId);

  EXPECT_CALL(policy, OnConsumerGroupRemoved(kConsumerId2, kChildId));
  policy_manager().OnConsumerGroupRemoved(kConsumerId2, kChildId);

  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

// Release requests are dropped while a group is suppressed, but limits still
// apply. Clearing the suppression asks the group to release memory.
TEST_F(MemoryCoordinatorPolicyManagerTest, MemoryReleaseSuppression) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  NiceMock<MockPolicy> policy(policy_manager());
  MemoryCoordinatorPolicyRegistration reg(policy_manager(), policy);

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kChildId);

  policy_manager().SetMemoryReleaseSuppression(&policy,
                                               MatchConsumerId(kConsumerId));

  // The limit is applied, but the release request is dropped.
  EXPECT_CALL(host,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId, base::MemoryLimit::FromPercent(50), false})));
  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId, {kConsumerId, base::MemoryLimit::FromPercent(50), true}}});
  Mock::VerifyAndClearExpectations(&host);

  // A release-only request is dropped entirely.
  EXPECT_CALL(host, UpdateConsumers(_)).Times(0);
  policy.manager().UpdateConsumers(
      &policy, {{kChildId, {kConsumerId, std::nullopt, true}}});
  Mock::VerifyAndClearExpectations(&host);

  // Clearing the suppression asks the group to release memory.
  EXPECT_CALL(host, UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                        kConsumerId, std::nullopt, true})));
  policy_manager().ClearMemoryReleaseSuppression(&policy);
  Mock::VerifyAndClearExpectations(&host);

  // Release requests are no longer dropped.
  EXPECT_CALL(host, UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                        kConsumerId, std::nullopt, true})));
  policy.manager().UpdateConsumers(
      &policy, {{kChildId, {kConsumerId, std::nullopt, true}}});
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

// Only the groups that match the filter are suppressed.
TEST_F(MemoryCoordinatorPolicyManagerTest, MemoryReleaseSuppression_Filter) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  NiceMock<MockPolicy> policy(policy_manager());
  MemoryCoordinatorPolicyRegistration reg(policy_manager(), policy);

  const char kConsumerName1[] = "consumer1";
  const uint32_t kConsumerId1 = base::PersistentHash(kConsumerName1);
  const char kConsumerName2[] = "consumer2";
  const uint32_t kConsumerId2 = base::PersistentHash(kConsumerName2);

  policy_manager().OnConsumerGroupAdded(kConsumerId1, kConsumerName1,
                                        kTestTraits1, kChildId);
  policy_manager().OnConsumerGroupAdded(kConsumerId2, kConsumerName2,
                                        kTestTraits1, kChildId);

  policy_manager().SetMemoryReleaseSuppression(&policy,
                                               MatchConsumerId(kConsumerId1));

  // Only the group that is not suppressed is asked to release memory.
  EXPECT_CALL(
      host, UpdateConsumers(ElementsAre(
                MemoryConsumerUpdate{kConsumerId1,
                                     base::MemoryLimit::FromPercent(50), false},
                MemoryConsumerUpdate{
                    kConsumerId2, base::MemoryLimit::FromPercent(50), true})));
  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId, {kConsumerId1, base::MemoryLimit::FromPercent(50), true}},
       {kChildId, {kConsumerId2, base::MemoryLimit::FromPercent(50), true}}});
  Mock::VerifyAndClearExpectations(&host);

  // Only the suppressed group is asked to release memory when the suppression
  // is cleared.
  EXPECT_CALL(host, UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                        kConsumerId1, std::nullopt, true})));
  policy_manager().ClearMemoryReleaseSuppression(&policy);
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId1, kChildId);
  policy_manager().OnConsumerGroupRemoved(kConsumerId2, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

// A group stays suppressed until all the policies that suppress it clear their
// suppression.
TEST_F(MemoryCoordinatorPolicyManagerTest, MemoryReleaseSuppression_Multiple) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  NiceMock<MockPolicy> policy(policy_manager());
  MemoryCoordinatorPolicyRegistration reg(policy_manager(), policy);

  NiceMock<MockPolicy> suppressing_policy1(policy_manager());
  MemoryCoordinatorPolicyRegistration suppressing_reg1(policy_manager(),
                                                       suppressing_policy1);

  NiceMock<MockPolicy> suppressing_policy2(policy_manager());
  MemoryCoordinatorPolicyRegistration suppressing_reg2(policy_manager(),
                                                       suppressing_policy2);

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kChildId);

  policy_manager().SetMemoryReleaseSuppression(&suppressing_policy1,
                                               MatchConsumerId(kConsumerId));
  policy_manager().SetMemoryReleaseSuppression(&suppressing_policy2,
                                               MatchConsumerId(kConsumerId));

  EXPECT_CALL(host,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId, base::MemoryLimit::FromPercent(50), false})));
  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId, {kConsumerId, base::MemoryLimit::FromPercent(50), true}}});
  Mock::VerifyAndClearExpectations(&host);

  // Clearing the first suppression doesn't stop suppressing releases for the
  // group.
  EXPECT_CALL(host, UpdateConsumers(_)).Times(0);
  policy_manager().ClearMemoryReleaseSuppression(&suppressing_policy1);
  policy.manager().UpdateConsumers(
      &policy, {{kChildId, {kConsumerId, std::nullopt, true}}});
  Mock::VerifyAndClearExpectations(&host);

  // Clearing the last suppression stops suppressing releases for the group.
  EXPECT_CALL(host, UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                        kConsumerId, std::nullopt, true})));
  policy_manager().ClearMemoryReleaseSuppression(&suppressing_policy2);
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

// Setting a new filter replaces the previous one. Groups that no longer match
// stop being suppressed.
TEST_F(MemoryCoordinatorPolicyManagerTest, MemoryReleaseSuppression_Replace) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  NiceMock<MockPolicy> policy(policy_manager());
  MemoryCoordinatorPolicyRegistration reg(policy_manager(), policy);

  const char kConsumerName1[] = "consumer1";
  const uint32_t kConsumerId1 = base::PersistentHash(kConsumerName1);
  const char kConsumerName2[] = "consumer2";
  const uint32_t kConsumerId2 = base::PersistentHash(kConsumerName2);

  policy_manager().OnConsumerGroupAdded(kConsumerId1, kConsumerName1,
                                        kTestTraits1, kChildId);
  policy_manager().OnConsumerGroupAdded(kConsumerId2, kConsumerName2,
                                        kTestTraits1, kChildId);

  policy_manager().SetMemoryReleaseSuppression(&policy,
                                               MatchConsumerId(kConsumerId1));

  // The release request for the first group is dropped.
  EXPECT_CALL(
      host, UpdateConsumers(ElementsAre(
                MemoryConsumerUpdate{kConsumerId1,
                                     base::MemoryLimit::FromPercent(50), false},
                MemoryConsumerUpdate{
                    kConsumerId2, base::MemoryLimit::FromPercent(50), true})));
  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId, {kConsumerId1, base::MemoryLimit::FromPercent(50), true}},
       {kChildId, {kConsumerId2, base::MemoryLimit::FromPercent(50), true}}});
  Mock::VerifyAndClearExpectations(&host);

  // Switching to the second group asks the first group to release memory.
  EXPECT_CALL(host, UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                        kConsumerId1, std::nullopt, true})));
  policy_manager().SetMemoryReleaseSuppression(&policy,
                                               MatchConsumerId(kConsumerId2));
  Mock::VerifyAndClearExpectations(&host);

  // Only the second group is now suppressed.
  EXPECT_CALL(host, UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                        kConsumerId1, std::nullopt, true})));
  policy.manager().UpdateConsumers(
      &policy, {{kChildId, {kConsumerId1, std::nullopt, true}},
                {kChildId, {kConsumerId2, std::nullopt, true}}});
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId1, kChildId);
  policy_manager().OnConsumerGroupRemoved(kConsumerId2, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

// Removing a policy clears its suppression. The dropped release request is
// sent along with the limit update, and only if still needed.
TEST_F(MemoryCoordinatorPolicyManagerTest,
       MemoryReleaseSuppression_RemovePolicy) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  NiceMock<MockPolicy> policy(policy_manager());
  MemoryCoordinatorPolicyRegistration reg(policy_manager(), policy);

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kChildId);

  // `policy` lowers the limit to 50%. `suppressing_policy` lowers it further to
  // 25% and suppresses the release request.
  EXPECT_CALL(host, UpdateConsumers(_)).Times(2);
  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId, {kConsumerId, base::MemoryLimit::FromPercent(50), false}}});
  {
    NiceMock<MockPolicy> suppressing_policy(policy_manager());
    MemoryCoordinatorPolicyRegistration suppressing_reg(policy_manager(),
                                                        suppressing_policy);
    policy_manager().SetMemoryReleaseSuppression(&suppressing_policy,
                                                 MatchConsumerId(kConsumerId));
    suppressing_policy.manager().UpdateConsumers(
        &suppressing_policy,
        {{kChildId, {kConsumerId, base::MemoryLimit::FromPercent(50), true}}});
    Mock::VerifyAndClearExpectations(&host);

    // Removing the policy restores the 50% limit. That is still lower than the
    // peak limit since the last release (100%), so the release is replayed in
    // the same update.
    EXPECT_CALL(host,
                UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                    kConsumerId, base::MemoryLimit::FromPercent(50), true})));
  }
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

// Clearing the suppression of a policy that doesn't suppress releases is a
// no-op.
TEST_F(MemoryCoordinatorPolicyManagerTest,
       MemoryReleaseSuppression_ClearWithoutSet) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  NiceMock<MockPolicy> policy(policy_manager());
  MemoryCoordinatorPolicyRegistration reg(policy_manager(), policy);

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kChildId);

  EXPECT_CALL(host, UpdateConsumers(_)).Times(0);
  policy_manager().ClearMemoryReleaseSuppression(&policy);
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

// Groups that are removed while suppressed are not notified when the
// suppression is cleared.
TEST_F(MemoryCoordinatorPolicyManagerTest,
       MemoryReleaseSuppression_GroupRemoved) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  NiceMock<MockPolicy> policy(policy_manager());
  MemoryCoordinatorPolicyRegistration reg(policy_manager(), policy);

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kChildId);

  policy_manager().SetMemoryReleaseSuppression(&policy,
                                               MatchConsumerId(kConsumerId));

  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);

  EXPECT_CALL(host, UpdateConsumers(_)).Times(0);
  policy_manager().ClearMemoryReleaseSuppression(&policy);
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

// Groups for which no release request was dropped are not asked to release
// memory when the suppression is cleared.
TEST_F(MemoryCoordinatorPolicyManagerTest,
       MemoryReleaseSuppression_NoDroppedRelease) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  NiceMock<MockPolicy> policy(policy_manager());
  MemoryCoordinatorPolicyRegistration reg(policy_manager(), policy);

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kChildId);

  policy_manager().SetMemoryReleaseSuppression(&policy,
                                               MatchConsumerId(kConsumerId));

  // Only a limit is requested while suppressed.
  EXPECT_CALL(host,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId, base::MemoryLimit::FromPercent(50), false})));
  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId, {kConsumerId, base::MemoryLimit::FromPercent(50), false}}});
  Mock::VerifyAndClearExpectations(&host);

  EXPECT_CALL(host, UpdateConsumers(_)).Times(0);
  policy_manager().ClearMemoryReleaseSuppression(&policy);
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

// A dropped release request is not replayed if the limit was restored to the
// highest limit since the group last released memory.
TEST_F(MemoryCoordinatorPolicyManagerTest,
       MemoryReleaseSuppression_LimitRestored) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  NiceMock<MockPolicy> policy(policy_manager());
  MemoryCoordinatorPolicyRegistration reg(policy_manager(), policy);

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kChildId);

  policy_manager().SetMemoryReleaseSuppression(&policy,
                                               MatchConsumerId(kConsumerId));

  // The release request is dropped.
  EXPECT_CALL(host,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId, base::MemoryLimit::FromPercent(50), false})));
  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId, {kConsumerId, base::MemoryLimit::FromPercent(50), true}}});
  Mock::VerifyAndClearExpectations(&host);

  // The limit is restored.
  EXPECT_CALL(host, UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                        kConsumerId, base::MemoryLimit::Default(), false})));
  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId, {kConsumerId, base::MemoryLimit::Default(), false}}});
  Mock::VerifyAndClearExpectations(&host);

  // The consumer can't be above its limit, so it is not asked to release
  // memory.
  EXPECT_CALL(host, UpdateConsumers(_)).Times(0);
  policy_manager().ClearMemoryReleaseSuppression(&policy);
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

// A dropped release request is replayed if the limit is lower than the highest
// limit since the group last released memory, even if above the default.
TEST_F(MemoryCoordinatorPolicyManagerTest,
       MemoryReleaseSuppression_AboveDefault) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  NiceMock<MockPolicy> policy(policy_manager());
  MemoryCoordinatorPolicyRegistration reg(policy_manager(), policy);

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kChildId);

  // The consumer is allowed to grow above the default.
  EXPECT_CALL(host,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId, base::MemoryLimit::FromPercent(150), false})));
  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId, {kConsumerId, base::MemoryLimit::FromPercent(150), false}}});
  Mock::VerifyAndClearExpectations(&host);

  policy_manager().SetMemoryReleaseSuppression(&policy,
                                               MatchConsumerId(kConsumerId));

  // The release request is dropped.
  EXPECT_CALL(host,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId, base::MemoryLimit::FromPercent(120), false})));
  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId, {kConsumerId, base::MemoryLimit::FromPercent(120), true}}});
  Mock::VerifyAndClearExpectations(&host);

  // The consumer may be above its limit, so it is asked to release memory.
  EXPECT_CALL(host, UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                        kConsumerId, std::nullopt, true})));
  policy_manager().ClearMemoryReleaseSuppression(&policy);
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

// A dropped release request is not replayed if the group already released
// memory at its current limit before being suppressed.
TEST_F(MemoryCoordinatorPolicyManagerTest,
       MemoryReleaseSuppression_ReleasedBeforeSuppression) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  NiceMock<MockPolicy> policy(policy_manager());
  MemoryCoordinatorPolicyRegistration reg(policy_manager(), policy);

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kChildId);

  // The release request is sent.
  EXPECT_CALL(host,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId, base::MemoryLimit::FromPercent(50), true})));
  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId, {kConsumerId, base::MemoryLimit::FromPercent(50), true}}});
  Mock::VerifyAndClearExpectations(&host);

  policy_manager().SetMemoryReleaseSuppression(&policy,
                                               MatchConsumerId(kConsumerId));

  // The release request is dropped.
  EXPECT_CALL(host, UpdateConsumers(_)).Times(0);
  policy.manager().UpdateConsumers(
      &policy, {{kChildId, {kConsumerId, std::nullopt, true}}});
  Mock::VerifyAndClearExpectations(&host);

  // The consumer already released memory at its current limit, so it is not
  // asked to release memory again.
  EXPECT_CALL(host, UpdateConsumers(_)).Times(0);
  policy_manager().ClearMemoryReleaseSuppression(&policy);
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

// A dropped release request is replayed if the limit was raised while
// suppressed, as the consumer may have grown above its current limit, even if
// it released memory at that limit before being suppressed.
TEST_F(MemoryCoordinatorPolicyManagerTest,
       MemoryReleaseSuppression_LimitRaisedWhileSuppressed) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  NiceMock<MockPolicy> policy(policy_manager());
  MemoryCoordinatorPolicyRegistration reg(policy_manager(), policy);

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kChildId);

  // The release request is sent.
  EXPECT_CALL(host,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId, base::MemoryLimit::FromPercent(50), true})));
  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId, {kConsumerId, base::MemoryLimit::FromPercent(50), true}}});
  Mock::VerifyAndClearExpectations(&host);

  policy_manager().SetMemoryReleaseSuppression(&policy,
                                               MatchConsumerId(kConsumerId));

  // The limit is raised, allowing the consumer to grow.
  EXPECT_CALL(host, UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                        kConsumerId, base::MemoryLimit::Default(), false})));
  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId, {kConsumerId, base::MemoryLimit::Default(), false}}});
  Mock::VerifyAndClearExpectations(&host);

  // The limit is lowered again, but the release request is dropped.
  EXPECT_CALL(host,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId, base::MemoryLimit::FromPercent(50), false})));
  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId, {kConsumerId, base::MemoryLimit::FromPercent(50), true}}});
  Mock::VerifyAndClearExpectations(&host);

  // The consumer may be above its limit, so it is asked to release memory.
  EXPECT_CALL(host, UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                        kConsumerId, std::nullopt, true})));
  policy_manager().ClearMemoryReleaseSuppression(&policy);
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

// A dropped release request is replayed for a stateless group as long as its
// limit is below the default, even if it already released memory at its
// current limit, as it ignores limit updates and may have grown since.
TEST_F(MemoryCoordinatorPolicyManagerTest,
       MemoryReleaseSuppression_StatelessReplayedUnderPressure) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  NiceMock<MockPolicy> policy(policy_manager());
  MemoryCoordinatorPolicyRegistration reg(policy_manager(), policy);

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kStatelessTraits, kChildId);

  // The release request is sent.
  EXPECT_CALL(host,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId, base::MemoryLimit::FromPercent(50), true})));
  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId, {kConsumerId, base::MemoryLimit::FromPercent(50), true}}});
  Mock::VerifyAndClearExpectations(&host);

  policy_manager().SetMemoryReleaseSuppression(&policy,
                                               MatchConsumerId(kConsumerId));

  // The limit is lowered, but the release request is dropped.
  EXPECT_CALL(host,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId, base::MemoryLimit::FromPercent(0), false})));
  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId, {kConsumerId, base::MemoryLimit::FromPercent(0), true}}});
  Mock::VerifyAndClearExpectations(&host);

  // The limit is raised back to where the group last released memory.
  EXPECT_CALL(host,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId, base::MemoryLimit::FromPercent(50), false})));
  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId, {kConsumerId, base::MemoryLimit::FromPercent(50), false}}});
  Mock::VerifyAndClearExpectations(&host);

  // The group is still under pressure, so it is asked to release memory.
  EXPECT_CALL(host, UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                        kConsumerId, std::nullopt, true})));
  policy_manager().ClearMemoryReleaseSuppression(&policy);
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

// Groups that declare themselves stateful are treated as stateless when
// base::kStatefulMemoryPressure is disabled, so a dropped release request is
// replayed as long as their limit is below the default.
TEST_F(MemoryCoordinatorPolicyManagerTest,
       MemoryReleaseSuppression_StatefulFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(base::kStatefulMemoryPressure);

  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  NiceMock<MockPolicy> policy(policy_manager());
  MemoryCoordinatorPolicyRegistration reg(policy_manager(), policy);

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  // `kTestTraits1` declares the group as stateful.
  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kChildId);

  // The release request is sent.
  EXPECT_CALL(host,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId, base::MemoryLimit::FromPercent(50), true})));
  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId, {kConsumerId, base::MemoryLimit::FromPercent(50), true}}});
  Mock::VerifyAndClearExpectations(&host);

  policy_manager().SetMemoryReleaseSuppression(&policy,
                                               MatchConsumerId(kConsumerId));

  // The release request is dropped.
  EXPECT_CALL(host, UpdateConsumers(_)).Times(0);
  policy.manager().UpdateConsumers(
      &policy, {{kChildId, {kConsumerId, std::nullopt, true}}});
  Mock::VerifyAndClearExpectations(&host);

  // Unlike in MemoryReleaseSuppression_ReleasedBeforeSuppression, the group is
  // asked to release memory.
  EXPECT_CALL(host, UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                        kConsumerId, std::nullopt, true})));
  policy_manager().ClearMemoryReleaseSuppression(&policy);
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

// Groups added while releases are suppressed are suppressed before policies are
// notified, so that release requests made in response are dropped.
TEST_F(MemoryCoordinatorPolicyObserverTest, MemoryReleaseSuppression_NewGroup) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(1);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);

  MockObserverPolicy policy(policy_manager());
  MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);

  NiceMock<MockPolicy> suppressing_policy(policy_manager());
  MemoryCoordinatorPolicyRegistration suppressing_registration(
      policy_manager(), suppressing_policy);

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  policy_manager().SetMemoryReleaseSuppression(&suppressing_policy,
                                               MatchConsumerId(kConsumerId));

  // The policy requests a limit and a release when the group is added. Only the
  // limit is applied.
  EXPECT_CALL(policy,
              OnConsumerGroupAdded(kConsumerId, kConsumerName, _, _, kChildId))
      .WillOnce([&]() {
        policy.manager().UpdateConsumers(
            &policy,
            {{kChildId,
              {kConsumerId, base::MemoryLimit::FromPercent(50), true}}});
      });
  EXPECT_CALL(host,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId, base::MemoryLimit::FromPercent(50), false})));
  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kChildId);
  Mock::VerifyAndClearExpectations(&policy);
  Mock::VerifyAndClearExpectations(&host);

  // Clearing the suppression asks the new group to release memory.
  EXPECT_CALL(host, UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                        kConsumerId, std::nullopt, true})));
  policy_manager().ClearMemoryReleaseSuppression(&suppressing_policy);
  Mock::VerifyAndClearExpectations(&host);

  // Clean up.
  EXPECT_CALL(policy, OnConsumerGroupRemoved(kConsumerId, kChildId));
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
TEST_F(MemoryCoordinatorPolicyManagerTest,
       AddDiagnosticObserverNotifiesExistingLimits) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId(42);
  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId,
                                              &host);
  policy_manager().OnConsumerGroupAdded(kConsumerId, kConsumerName,
                                        kTestTraits1, kChildId);

  MockPolicy policy(policy_manager());
  MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);

  // Set an initial limit.
  EXPECT_CALL(host,
              UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                  kConsumerId, base::MemoryLimit::FromPercent(50), false})));
  policy.manager().UpdateConsumers(
      &policy,
      {{kChildId, {kConsumerId, base::MemoryLimit::FromPercent(50), false}}});
  Mock::VerifyAndClearExpectations(&host);

  // Adding a diagnostic observer should immediately notify the current limit.
  MockDiagnosticObserver observer;
  EXPECT_CALL(observer, OnMemoryLimitChanged(
                            kConsumerId, kChildId,
                            base::MemoryLimit::ModeratePressureThreshold()));
  policy_manager().AddDiagnosticObserver(&observer);
  Mock::VerifyAndClearExpectations(&observer);

  // Clean up.
  policy_manager().RemoveDiagnosticObserver(&observer);
  policy_manager().OnConsumerGroupRemoved(kConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}
#endif

}  // namespace content
