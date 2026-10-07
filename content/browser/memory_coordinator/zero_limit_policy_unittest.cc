// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory_coordinator/zero_limit_policy.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "base/hash/hash.h"
#include "base/memory_coordinator/memory_limit.h"
#include "base/memory_coordinator/mock_memory_consumer.h"
#include "base/memory_coordinator/traits.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "content/browser/memory_coordinator/browser_memory_coordinator_impl.h"
#include "content/common/memory_coordinator/memory_consumer_group_host.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/memory_consumer_update.h"
#include "content/public/common/process_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using ::testing::Mock;
using ::testing::UnorderedElementsAre;

constexpr base::MemoryConsumerTraits kStatefulTraits(
    base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
    base::MemoryConsumerTraits::ReleaseMemoryCost::kRequiresTraversal,
    base::MemoryConsumerTraits::InformationRetention::kLossless,
    base::MemoryConsumerTraits::ExecutionType::kSynchronous,
    base::MemoryConsumerTraits::IsStateful::kYes);

constexpr base::MemoryConsumerTraits kStatelessTraits(
    base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
    base::MemoryConsumerTraits::ReleaseMemoryCost::kRequiresTraversal,
    base::MemoryConsumerTraits::InformationRetention::kLossless,
    base::MemoryConsumerTraits::ExecutionType::kSynchronous,
    base::MemoryConsumerTraits::IsStateful::kNo);

class MockMemoryConsumerGroupHost : public MemoryConsumerGroupHost {
 public:
  MOCK_METHOD(void,
              UpdateConsumers,
              (std::vector<MemoryConsumerUpdate> updates),
              (override));
  MOCK_METHOD(void,
              SetOverrideLimit,
              (uint32_t, base::MemoryLimit),
              (override));
  MOCK_METHOD(void,
              ClearOverrideLimit,
              (uint32_t, base::MemoryLimit),
              (override));
};

}  // namespace

class ZeroLimitPolicyTest : public testing::Test {
 protected:
  MemoryCoordinatorPolicyManager& policy_manager() { return policy_manager_; }

 private:
  base::test::TaskEnvironment task_environment_;
  MemoryCoordinatorPolicyManager policy_manager_;
};

TEST_F(ZeroLimitPolicyTest, ExistingConsumersUpdatedToZero) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId;

  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_BROWSER, kChildId,
                                              &host);

  const std::string kStatefulName = "stateful_consumer";
  const uint32_t kStatefulId = base::PersistentHash(kStatefulName);
  const std::string kStatelessName = "stateless_consumer";
  const uint32_t kStatelessId = base::PersistentHash(kStatelessName);

  policy_manager().OnConsumerGroupAdded(kStatefulId, kStatefulName,
                                        kStatefulTraits, kChildId);
  policy_manager().OnConsumerGroupAdded(kStatelessId, kStatelessName,
                                        kStatelessTraits, kChildId);

  // Instantiating the policy should update only the stateful consumer to 0% and
  // request memory release. The stateless consumer is unaffected.
  {
    EXPECT_CALL(host,
                UpdateConsumers(UnorderedElementsAre(MemoryConsumerUpdate{
                    kStatefulId, base::MemoryLimit::FromPercent(0), true})));
    ZeroLimitPolicy policy(policy_manager());
    Mock::VerifyAndClearExpectations(&host);

    // When the policy is destroyed, the stateful consumer's limit reverts to
    // default (100%).
    EXPECT_CALL(host, UpdateConsumers(UnorderedElementsAre(MemoryConsumerUpdate{
                          kStatefulId, base::MemoryLimit::Default(), false})));
  }
  Mock::VerifyAndClearExpectations(&host);

  policy_manager().OnConsumerGroupRemoved(kStatefulId, kChildId);
  policy_manager().OnConsumerGroupRemoved(kStatelessId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

TEST_F(ZeroLimitPolicyTest, NewConsumersReceiveZeroLimit) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId;

  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_BROWSER, kChildId,
                                              &host);

  ZeroLimitPolicy policy(policy_manager());

  const std::string kStatefulName = "stateful_consumer";
  const uint32_t kStatefulId = base::PersistentHash(kStatefulName);
  const std::string kStatelessName = "stateless_consumer";
  const uint32_t kStatelessId = base::PersistentHash(kStatelessName);

  // A stateful consumer added after the policy is created must immediately
  // receive 0% limit and release memory.
  EXPECT_CALL(host,
              UpdateConsumers(UnorderedElementsAre(MemoryConsumerUpdate{
                  kStatefulId, base::MemoryLimit::FromPercent(0), true})));
  policy_manager().OnConsumerGroupAdded(kStatefulId, kStatefulName,
                                        kStatefulTraits, kChildId);
  Mock::VerifyAndClearExpectations(&host);

  // A stateless consumer added after the policy is created is not updated.
  EXPECT_CALL(host, UpdateConsumers).Times(0);
  policy_manager().OnConsumerGroupAdded(kStatelessId, kStatelessName,
                                        kStatelessTraits, kChildId);
  Mock::VerifyAndClearExpectations(&host);

  policy_manager().OnConsumerGroupRemoved(kStatefulId, kChildId);
  policy_manager().OnConsumerGroupRemoved(kStatelessId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

TEST_F(ZeroLimitPolicyTest, MultipleProcesses) {
  MockMemoryConsumerGroupHost browser_host;
  MockMemoryConsumerGroupHost child_host;
  const ChildProcessId kBrowserChildId;
  const ChildProcessId kChildProcessId(1);

  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_BROWSER,
                                              kBrowserChildId, &browser_host);
  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER,
                                              kChildProcessId, &child_host);

  const std::string kBrowserConsumer = "browser_consumer";
  const uint32_t kBrowserConsumerId = base::PersistentHash(kBrowserConsumer);
  const std::string kChildConsumer = "child_consumer";
  const uint32_t kChildConsumerId = base::PersistentHash(kChildConsumer);

  policy_manager().OnConsumerGroupAdded(kBrowserConsumerId, kBrowserConsumer,
                                        kStatefulTraits, kBrowserChildId);
  policy_manager().OnConsumerGroupAdded(kChildConsumerId, kChildConsumer,
                                        kStatefulTraits, kChildProcessId);

  // Instantiating the policy updates stateful consumers in both processes.
  EXPECT_CALL(
      browser_host,
      UpdateConsumers(UnorderedElementsAre(MemoryConsumerUpdate{
          kBrowserConsumerId, base::MemoryLimit::FromPercent(0), true})));
  EXPECT_CALL(child_host,
              UpdateConsumers(UnorderedElementsAre(MemoryConsumerUpdate{
                  kChildConsumerId, base::MemoryLimit::FromPercent(0), true})));
  ZeroLimitPolicy policy(policy_manager());
  Mock::VerifyAndClearExpectations(&browser_host);
  Mock::VerifyAndClearExpectations(&child_host);

  policy_manager().OnConsumerGroupRemoved(kBrowserConsumerId, kBrowserChildId);
  policy_manager().OnConsumerGroupRemoved(kChildConsumerId, kChildProcessId);
  policy_manager().RemoveMemoryConsumerGroupHost(kBrowserChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildProcessId);
}

TEST_F(ZeroLimitPolicyTest, BrowserMemoryCoordinatorIntegration) {
  // When feature is enabled, stateful consumers registered with
  // BrowserMemoryCoordinator receive a 0% limit, while stateless consumers
  // remain at default 100%.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(kMemoryCoordinatorZeroLimit);
    BrowserMemoryCoordinatorImpl coordinator;
    coordinator.InitializePolicies();
    base::RegisteredMockMemoryConsumer stateful_consumer("stateful",
                                                         kStatefulTraits);
    base::RegisteredMockMemoryConsumer stateless_consumer("stateless",
                                                          kStatelessTraits);
    EXPECT_EQ(stateful_consumer.memory_limit(),
              base::MemoryLimit::FromPercent(0));
    EXPECT_EQ(stateless_consumer.memory_limit(), base::MemoryLimit::Default());
  }

  // When feature is disabled, all consumers remain at default 100%.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(kMemoryCoordinatorZeroLimit);
    BrowserMemoryCoordinatorImpl coordinator;
    coordinator.InitializePolicies();
    base::RegisteredMockMemoryConsumer stateful_consumer("stateful",
                                                         kStatefulTraits);
    base::RegisteredMockMemoryConsumer stateless_consumer("stateless",
                                                          kStatelessTraits);
    EXPECT_EQ(stateful_consumer.memory_limit(), base::MemoryLimit::Default());
    EXPECT_EQ(stateless_consumer.memory_limit(), base::MemoryLimit::Default());
  }
}

}  // namespace content
