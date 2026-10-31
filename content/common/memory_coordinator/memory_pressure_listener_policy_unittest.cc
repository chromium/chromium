// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/memory_coordinator/memory_pressure_listener_policy.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/hash/hash.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/memory_pressure_listener_registry.h"
#include "base/memory_coordinator/memory_coordinator_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "content/common/memory_coordinator/memory_consumer_group_host.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"
#include "content/public/common/memory_consumer_update.h"
#include "content/public/common/memory_coordinator/memory_coordinator_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Mock;
using ::testing::UnorderedElementsAre;

constexpr base::MemoryConsumerTraits kTestTraits(
    base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
    base::MemoryConsumerTraits::ReleaseMemoryDataAccess::kReads,
    base::MemoryConsumerTraits::InformationRetention::kLossless,
    base::MemoryConsumerTraits::ExecutionType::kSynchronous);

constexpr base::MemoryConsumerTraits kDiskWritingTraits(
    base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
    base::MemoryConsumerTraits::ReleaseMemoryDataAccess::kReadsAndWritesToDisk,
    base::MemoryConsumerTraits::InformationRetention::kLossless,
    base::MemoryConsumerTraits::ExecutionType::kSynchronous);

constexpr char kConsumerName[] = "consumer";
constexpr char kDiskWritingConsumerName[] = "disk_writing_consumer";

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

class MemoryPressureListenerPolicyTest : public testing::Test {
 protected:
  MemoryCoordinatorPolicyManager& policy_manager() { return policy_manager_; }

  const uint32_t consumer_id_ = base::PersistentHash(kConsumerName);
  const uint32_t disk_writing_consumer_id_ =
      base::PersistentHash(kDiskWritingConsumerName);

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
  EXPECT_CALL(
      host, UpdateConsumers(UnorderedElementsAre(
                MemoryConsumerUpdate{kConsumerId1,
                                     base::MemoryLimit::FromPercent(50), true},
                MemoryConsumerUpdate{
                    kConsumerId2, base::MemoryLimit::FromPercent(50), true})));
  base::MemoryPressureListener::SimulatePressureNotification(
      base::MEMORY_PRESSURE_LEVEL_MODERATE);
  Mock::VerifyAndClearExpectations(&host);

  // Critical pressure: 0% limit and release memory.
  EXPECT_CALL(host,
              UpdateConsumers(UnorderedElementsAre(
                  MemoryConsumerUpdate{kConsumerId1,
                                       base::MemoryLimit::FromPercent(0), true},
                  MemoryConsumerUpdate{
                      kConsumerId2, base::MemoryLimit::FromPercent(0), true})));
  base::MemoryPressureListener::SimulatePressureNotification(
      base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  Mock::VerifyAndClearExpectations(&host);

  // No pressure: 100% limit and do not release memory.
  EXPECT_CALL(
      host, UpdateConsumers(UnorderedElementsAre(
                MemoryConsumerUpdate{kConsumerId1, base::MemoryLimit::Default(),
                                     false},
                MemoryConsumerUpdate{kConsumerId2, base::MemoryLimit::Default(),
                                     false})));
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

  {
    MemoryPressureListenerPolicy policy(policy_manager());
    MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);

    // Moderate pressure: 50% limit and release memory.
    base::MemoryPressureListener::SimulatePressureNotification(
        base::MEMORY_PRESSURE_LEVEL_MODERATE);

    // Consumers added AFTER the pressure event should immediately receive the
    // limit that was set.
    EXPECT_CALL(host,
                UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                    consumer_id_, base::MemoryLimit::FromPercent(50), true})));
    policy_manager().OnConsumerGroupAdded(consumer_id_, kConsumerName,
                                          kTestTraits, kChildId);
    Mock::VerifyAndClearExpectations(&host);

    EXPECT_CALL(host, UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                          disk_writing_consumer_id_,
                          base::MemoryLimit::FromPercent(50), true})));
    policy_manager().OnConsumerGroupAdded(disk_writing_consumer_id_,
                                          kDiskWritingConsumerName,
                                          kDiskWritingTraits, kChildId);
    Mock::VerifyAndClearExpectations(&host);

    // Removing the policy should reset the limits to default (100%).
    EXPECT_CALL(
        host, UpdateConsumers(UnorderedElementsAre(
                  MemoryConsumerUpdate{consumer_id_,
                                       base::MemoryLimit::Default(), false},
                  MemoryConsumerUpdate{disk_writing_consumer_id_,
                                       base::MemoryLimit::Default(), false})));
  }
  Mock::VerifyAndClearExpectations(&host);

  policy_manager().OnConsumerGroupRemoved(consumer_id_, kChildId);
  policy_manager().OnConsumerGroupRemoved(disk_writing_consumer_id_, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

TEST_F(MemoryPressureListenerPolicyTest,
       MemoryCacheSkippedWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      blink::features::kReleaseResourceStrongReferencesOnMemoryPressure);

  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId;

  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_BROWSER, kChildId,
                                              &host);

  const std::string kMemoryCacheName = "MemoryCache";
  const uint32_t kMemoryCacheId = base::PersistentHash(kMemoryCacheName);
  const std::string kOtherConsumerName = "other_consumer";
  const uint32_t kOtherConsumerId = base::PersistentHash(kOtherConsumerName);

  policy_manager().OnConsumerGroupAdded(kMemoryCacheId, kMemoryCacheName,
                                        kTestTraits, kChildId);
  policy_manager().OnConsumerGroupAdded(kOtherConsumerId, kOtherConsumerName,
                                        kTestTraits, kChildId);

  MemoryPressureListenerPolicy policy(policy_manager());
  MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);

  // When feature is disabled, MemoryCache is skipped, but other consumer is
  // updated.
  EXPECT_CALL(
      host, UpdateConsumers(UnorderedElementsAre(MemoryConsumerUpdate{
                kOtherConsumerId, base::MemoryLimit::FromPercent(50), true})));
  base::MemoryPressureListener::SimulatePressureNotification(
      base::MEMORY_PRESSURE_LEVEL_MODERATE);
  Mock::VerifyAndClearExpectations(&host);

  policy_manager().OnConsumerGroupRemoved(kMemoryCacheId, kChildId);
  policy_manager().OnConsumerGroupRemoved(kOtherConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

TEST_F(MemoryPressureListenerPolicyTest,
       MemoryCacheIncludedWhenFeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kReleaseResourceStrongReferencesOnMemoryPressure);

  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId;

  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_BROWSER, kChildId,
                                              &host);

  const std::string kMemoryCacheName = "MemoryCache";
  const uint32_t kMemoryCacheId = base::PersistentHash(kMemoryCacheName);
  const std::string kOtherConsumerName = "other_consumer";
  const uint32_t kOtherConsumerId = base::PersistentHash(kOtherConsumerName);

  policy_manager().OnConsumerGroupAdded(kMemoryCacheId, kMemoryCacheName,
                                        kTestTraits, kChildId);
  policy_manager().OnConsumerGroupAdded(kOtherConsumerId, kOtherConsumerName,
                                        kTestTraits, kChildId);

  MemoryPressureListenerPolicy policy(policy_manager());
  MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);

  // When feature is enabled, MemoryCache is included along with other
  // consumers.
  EXPECT_CALL(
      host,
      UpdateConsumers(UnorderedElementsAre(
          MemoryConsumerUpdate{kMemoryCacheId,
                               base::MemoryLimit::FromPercent(50), true},
          MemoryConsumerUpdate{kOtherConsumerId,
                               base::MemoryLimit::FromPercent(50), true})));
  base::MemoryPressureListener::SimulatePressureNotification(
      base::MEMORY_PRESSURE_LEVEL_MODERATE);
  Mock::VerifyAndClearExpectations(&host);

  policy_manager().OnConsumerGroupRemoved(kMemoryCacheId, kChildId);
  policy_manager().OnConsumerGroupRemoved(kOtherConsumerId, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

// Consumers that release memory by writing it to disk get the critical pressure
// limit when the pressure goes from moderate to critical, but are not asked to
// release memory. They are not asked to release memory when the pressure goes
// back to none either, as they can't be above their restored limit.
TEST_F(MemoryPressureListenerPolicyTest,
       DiskWritingConsumersNotReleasedOnCritical) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId;  // Current process

  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_BROWSER, kChildId,
                                              &host);

  policy_manager().OnConsumerGroupAdded(consumer_id_, kConsumerName,
                                        kTestTraits, kChildId);
  policy_manager().OnConsumerGroupAdded(disk_writing_consumer_id_,
                                        kDiskWritingConsumerName,
                                        kDiskWritingTraits, kChildId);

  MemoryPressureListenerPolicy policy(policy_manager());
  MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);

  // Moderate pressure: both consumers get a 50% limit and release memory.
  EXPECT_CALL(
      host,
      UpdateConsumers(UnorderedElementsAre(
          MemoryConsumerUpdate{consumer_id_, base::MemoryLimit::FromPercent(50),
                               true},
          MemoryConsumerUpdate{disk_writing_consumer_id_,
                               base::MemoryLimit::FromPercent(50), true})));
  base::MemoryPressureListener::SimulatePressureNotification(
      base::MEMORY_PRESSURE_LEVEL_MODERATE);
  Mock::VerifyAndClearExpectations(&host);

  // Critical pressure: both consumers get a 0% limit, but only the other
  // consumer is asked to release memory.
  EXPECT_CALL(
      host,
      UpdateConsumers(UnorderedElementsAre(
          MemoryConsumerUpdate{consumer_id_, base::MemoryLimit::FromPercent(0),
                               true},
          MemoryConsumerUpdate{disk_writing_consumer_id_,
                               base::MemoryLimit::FromPercent(0), false})));
  base::MemoryPressureListener::SimulatePressureNotification(
      base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  Mock::VerifyAndClearExpectations(&host);

  // No pressure: both consumers get their limit restored, and neither is asked
  // to release memory.
  EXPECT_CALL(
      host, UpdateConsumers(UnorderedElementsAre(
                MemoryConsumerUpdate{consumer_id_, base::MemoryLimit::Default(),
                                     false},
                MemoryConsumerUpdate{disk_writing_consumer_id_,
                                     base::MemoryLimit::Default(), false})));
  base::MemoryPressureListener::SimulatePressureNotification(
      base::MEMORY_PRESSURE_LEVEL_NONE);
  Mock::VerifyAndClearExpectations(&host);

  policy_manager().OnConsumerGroupRemoved(consumer_id_, kChildId);
  policy_manager().OnConsumerGroupRemoved(disk_writing_consumer_id_, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

// Consumers that release memory by writing it to disk are not asked to release
// memory when the pressure goes straight from none to critical. They are asked
// once the pressure goes down to moderate.
TEST_F(MemoryPressureListenerPolicyTest,
       DiskWritingConsumersNotReleasedOnCriticalFromNoPressure) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId;  // Current process

  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_BROWSER, kChildId,
                                              &host);

  policy_manager().OnConsumerGroupAdded(consumer_id_, kConsumerName,
                                        kTestTraits, kChildId);
  policy_manager().OnConsumerGroupAdded(disk_writing_consumer_id_,
                                        kDiskWritingConsumerName,
                                        kDiskWritingTraits, kChildId);

  MemoryPressureListenerPolicy policy(policy_manager());
  MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);

  // Critical pressure: both consumers get a 0% limit, but only the other
  // consumer is asked to release memory.
  EXPECT_CALL(
      host,
      UpdateConsumers(UnorderedElementsAre(
          MemoryConsumerUpdate{consumer_id_, base::MemoryLimit::FromPercent(0),
                               true},
          MemoryConsumerUpdate{disk_writing_consumer_id_,
                               base::MemoryLimit::FromPercent(0), false})));
  base::MemoryPressureListener::SimulatePressureNotification(
      base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  Mock::VerifyAndClearExpectations(&host);

  // Moderate pressure: both consumers get a 50% limit. The release request is
  // still suppressed for the disk-writing consumer when the limit is lowered,
  // but it is asked to release memory once no longer suppressed.
  EXPECT_CALL(
      host,
      UpdateConsumers(UnorderedElementsAre(
          MemoryConsumerUpdate{consumer_id_, base::MemoryLimit::FromPercent(50),
                               true},
          MemoryConsumerUpdate{disk_writing_consumer_id_,
                               base::MemoryLimit::FromPercent(50), false})));
  EXPECT_CALL(host, UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                        disk_writing_consumer_id_, std::nullopt, true})));
  base::MemoryPressureListener::SimulatePressureNotification(
      base::MEMORY_PRESSURE_LEVEL_MODERATE);
  Mock::VerifyAndClearExpectations(&host);

  // No pressure: both consumers get their limit restored.
  EXPECT_CALL(
      host, UpdateConsumers(UnorderedElementsAre(
                MemoryConsumerUpdate{consumer_id_, base::MemoryLimit::Default(),
                                     false},
                MemoryConsumerUpdate{disk_writing_consumer_id_,
                                     base::MemoryLimit::Default(), false})));
  base::MemoryPressureListener::SimulatePressureNotification(
      base::MEMORY_PRESSURE_LEVEL_NONE);
  Mock::VerifyAndClearExpectations(&host);

  policy_manager().OnConsumerGroupRemoved(consumer_id_, kChildId);
  policy_manager().OnConsumerGroupRemoved(disk_writing_consumer_id_, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

// Consumers that release memory by writing it to disk and are added while under
// critical pressure get the critical pressure limit, but are not asked to
// release memory.
TEST_F(MemoryPressureListenerPolicyTest,
       DiskWritingConsumerAddedUnderCriticalNotReleased) {
  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId;  // Current process

  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_BROWSER, kChildId,
                                              &host);

  MemoryPressureListenerPolicy policy(policy_manager());
  MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);

  // No consumers yet, so no updates.
  EXPECT_CALL(host, UpdateConsumers(_)).Times(0);
  base::MemoryPressureListener::SimulatePressureNotification(
      base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  Mock::VerifyAndClearExpectations(&host);

  // The disk-writing consumer gets a 0% limit but is not asked to release
  // memory.
  EXPECT_CALL(host, UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                        disk_writing_consumer_id_,
                        base::MemoryLimit::FromPercent(0), false})));
  policy_manager().OnConsumerGroupAdded(disk_writing_consumer_id_,
                                        kDiskWritingConsumerName,
                                        kDiskWritingTraits, kChildId);
  Mock::VerifyAndClearExpectations(&host);

  policy_manager().OnConsumerGroupRemoved(disk_writing_consumer_id_, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

// Repeated critical pressure notifications re-trigger the release for stateless
// consumers, except for those that release memory by writing it to disk.
TEST_F(MemoryPressureListenerPolicyTest,
       DiskWritingConsumersNotReleasedOnRepeatedCritical) {
  // Disabling kStatefulMemoryPressure makes all consumers stateless, so that
  // they are eligible for repeated releases.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(base::kStatefulMemoryPressure);

  MockMemoryConsumerGroupHost host;
  const ChildProcessId kChildId;  // Current process

  policy_manager().AddMemoryConsumerGroupHost(PROCESS_TYPE_BROWSER, kChildId,
                                              &host);

  policy_manager().OnConsumerGroupAdded(consumer_id_, kConsumerName,
                                        kTestTraits, kChildId);
  policy_manager().OnConsumerGroupAdded(disk_writing_consumer_id_,
                                        kDiskWritingConsumerName,
                                        kDiskWritingTraits, kChildId);

  MemoryPressureListenerPolicy policy(policy_manager());
  MemoryCoordinatorPolicyRegistration registration(policy_manager(), policy);

  // Critical pressure: both consumers get a 0% limit, but only the other
  // consumer is asked to release memory.
  EXPECT_CALL(
      host,
      UpdateConsumers(UnorderedElementsAre(
          MemoryConsumerUpdate{consumer_id_, base::MemoryLimit::FromPercent(0),
                               true},
          MemoryConsumerUpdate{disk_writing_consumer_id_,
                               base::MemoryLimit::FromPercent(0), false})));
  base::MemoryPressureListener::SimulatePressureNotification(
      base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  Mock::VerifyAndClearExpectations(&host);

  // Repeated critical pressure: only the other consumer is asked to release
  // memory again.
  EXPECT_CALL(host, UpdateConsumers(ElementsAre(MemoryConsumerUpdate{
                        consumer_id_, std::nullopt, true})));
  base::MemoryPressureListener::SimulatePressureNotification(
      base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  Mock::VerifyAndClearExpectations(&host);

  policy_manager().OnConsumerGroupRemoved(consumer_id_, kChildId);
  policy_manager().OnConsumerGroupRemoved(disk_writing_consumer_id_, kChildId);
  policy_manager().RemoveMemoryConsumerGroupHost(kChildId);
}

}  // namespace content
