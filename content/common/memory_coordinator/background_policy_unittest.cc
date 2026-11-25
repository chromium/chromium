// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/memory_coordinator/background_policy.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/process/process.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "content/common/memory_coordinator/memory_consumer_group_host.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"
#include "content/public/common/memory_consumer_update.h"
#include "content/public/common/memory_coordinator/predicate_memory_coordinator_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/pre_freeze_background_memory_trimmer.h"
#endif

namespace content {

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Mock;
using ::testing::UnorderedElementsAreArray;

using Traits = base::MemoryConsumerTraits;

constexpr base::Process::Priority kBackgroundPriority =
    base::Process::Priority::kBestEffort;
constexpr base::Process::Priority kForegroundPriority =
    base::Process::Priority::kUserBlocking;

constexpr Traits kShortDelayTraits(
    Traits::EstimatedMemoryUsage::kSmall,
    Traits::ReleaseMemoryDataAccess::kReads,
    Traits::InformationRetention::kLossless,
    Traits::ExecutionType::kSynchronous,
    Traits::AffectedByBackgrounding::kAfterShortDelay);

constexpr Traits kMediumDelayTraits(
    Traits::EstimatedMemoryUsage::kSmall,
    Traits::ReleaseMemoryDataAccess::kReads,
    Traits::InformationRetention::kLossless,
    Traits::ExecutionType::kSynchronous,
    Traits::AffectedByBackgrounding::kAfterMediumDelay);

constexpr Traits kLongDelayTraits(
    Traits::EstimatedMemoryUsage::kSmall,
    Traits::ReleaseMemoryDataAccess::kReads,
    Traits::InformationRetention::kLossless,
    Traits::ExecutionType::kSynchronous,
    Traits::AffectedByBackgrounding::kAfterLongDelay);

constexpr Traits kNoBackgroundTraits(Traits::EstimatedMemoryUsage::kSmall,
                                     Traits::ReleaseMemoryDataAccess::kReads,
                                     Traits::InformationRetention::kLossless,
                                     Traits::ExecutionType::kSynchronous);

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

MemoryConsumerUpdate LimitUpdate(uint32_t consumer_id,
                                 base::MemoryLimit memory_limit) {
  return {consumer_id, memory_limit, /*release_memory=*/false};
}

MemoryConsumerUpdate ReleaseUpdate(uint32_t consumer_id) {
  return {consumer_id, std::nullopt, /*release_memory=*/true};
}

MemoryConsumerUpdate LimitAndReleaseUpdate(uint32_t consumer_id,
                                           base::MemoryLimit memory_limit) {
  return {consumer_id, memory_limit, /*release_memory=*/true};
}

}  // namespace

class BackgroundPolicyTest : public testing::Test {
 protected:
  void SetUp() override {
    policy_.emplace(policy_manager_);
    policy_manager_.AddMemoryConsumerGroupHost(PROCESS_TYPE_BROWSER,
                                               kLocalProcessId, &host_);
    AddConsumer(kShortId, kShortName, kShortDelayTraits);
    AddConsumer(kMediumId, kMediumName, kMediumDelayTraits);
    AddConsumer(kLongId, kLongName, kLongDelayTraits);
    AddConsumer(kNoBackgroundId, kNoBackgroundName, kNoBackgroundTraits);
  }

  void TearDown() override {
    // Removing the policy restores the limit of consumers if backgrounded.
    EXPECT_CALL(host_, UpdateConsumers(_)).Times(testing::AnyNumber());
    policy_.reset();
    for (uint32_t consumer_id : consumer_ids_) {
      policy_manager_.OnConsumerGroupRemoved(consumer_id, kLocalProcessId);
    }
    policy_manager_.RemoveMemoryConsumerGroupHost(kLocalProcessId);
  }

  void AddConsumer(uint32_t consumer_id,
                   std::string_view consumer_name,
                   Traits traits) {
    consumer_ids_.push_back(consumer_id);
    policy_manager_.OnConsumerGroupAdded(consumer_id, consumer_name, traits,
                                         kLocalProcessId);
  }

  BackgroundPolicy& policy() { return *policy_; }

  // Expects exactly `updates` to be sent by `action`, or no update if empty.
  template <typename Action>
  void ExpectUpdates(std::vector<MemoryConsumerUpdate> updates, Action action) {
    if (updates.empty()) {
      EXPECT_CALL(host_, UpdateConsumers(_)).Times(0);
    } else {
      EXPECT_CALL(host_, UpdateConsumers(
                             UnorderedElementsAreArray(std::move(updates))));
    }
    action();
    Mock::VerifyAndClearExpectations(&host_);
  }

  // Backgrounds the policy and verifies that every consumer that opted in got
  // the background limit.
  void Background() {
    ExpectUpdates(
        {LimitUpdate(kShortId, BackgroundPolicy::kBackgroundLimit),
         LimitUpdate(kMediumId, BackgroundPolicy::kBackgroundLimit),
         LimitUpdate(kLongId, BackgroundPolicy::kBackgroundLimit)},
        [&] { policy().OnProcessPriorityChanged(kBackgroundPriority); });
  }

  // Foregrounds the policy and verifies that the limit of every consumer that
  // opted in is restored.
  void Foreground() {
    ExpectUpdates(
        {LimitUpdate(kShortId, base::MemoryLimit::Default()),
         LimitUpdate(kMediumId, base::MemoryLimit::Default()),
         LimitUpdate(kLongId, base::MemoryLimit::Default())},
        [&] { policy().OnProcessPriorityChanged(kForegroundPriority); });
  }

  // Advances the mock time by `delta` and verifies that `updates` are the only
  // updates sent.
  void FastForwardAndExpect(base::TimeDelta delta,
                            std::vector<MemoryConsumerUpdate> updates) {
    ExpectUpdates(std::move(updates),
                  [&] { task_environment_.FastForwardBy(delta); });
  }

  const std::string kShortName = "short";
  const uint32_t kShortId = base::PersistentHash(kShortName);
  const std::string kMediumName = "medium";
  const uint32_t kMediumId = base::PersistentHash(kMediumName);
  const std::string kLongName = "long";
  const uint32_t kLongId = base::PersistentHash(kLongName);
  const std::string kNoBackgroundName = "no_background";
  const uint32_t kNoBackgroundId = base::PersistentHash(kNoBackgroundName);

  const ChildProcessId kLocalProcessId;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  testing::StrictMock<MockMemoryConsumerGroupHost> host_;
  MemoryCoordinatorPolicyManager policy_manager_;
  std::vector<uint32_t> consumer_ids_;
  std::optional<BackgroundPolicy> policy_;
};

TEST_F(BackgroundPolicyTest, BackgroundAndForeground) {
  Background();

  // Repeated calls are ignored.
  policy().OnProcessPriorityChanged(kBackgroundPriority);

  Foreground();

  // Repeated calls are ignored.
  policy().OnProcessPriorityChanged(kForegroundPriority);
}

TEST_F(BackgroundPolicyTest, ReleaseAfterDelay) {
  Background();

  FastForwardAndExpect(BackgroundPolicy::kShortDelay,
                       {ReleaseUpdate(kShortId)});
  FastForwardAndExpect(
      BackgroundPolicy::kMediumDelay - BackgroundPolicy::kShortDelay,
      {ReleaseUpdate(kMediumId)});
  FastForwardAndExpect(
      BackgroundPolicy::kLongDelay - BackgroundPolicy::kMediumDelay,
      {ReleaseUpdate(kLongId)});

  // Nothing else happens.
  FastForwardAndExpect(BackgroundPolicy::kLongDelay, {});

  Foreground();
}

TEST_F(BackgroundPolicyTest, ForegroundCancelsRelease) {
  Background();
  Foreground();

  FastForwardAndExpect(BackgroundPolicy::kLongDelay, {});
}

TEST_F(BackgroundPolicyTest, ForegroundAfterSomeDelaysExpired) {
  Background();
  FastForwardAndExpect(BackgroundPolicy::kShortDelay,
                       {ReleaseUpdate(kShortId)});

  Foreground();

  FastForwardAndExpect(BackgroundPolicy::kLongDelay, {});
}

TEST_F(BackgroundPolicyTest, ConsumerAddedWhileBackgrounded) {
  Background();

  // A consumer added while backgrounded gets the background limit, but isn't
  // asked to release memory until its delay expires.
  const std::string kLateName = "late";
  const uint32_t kLateId = base::PersistentHash(kLateName);
  ExpectUpdates({LimitUpdate(kLateId, BackgroundPolicy::kBackgroundLimit)},
                [&] { AddConsumer(kLateId, kLateName, kShortDelayTraits); });

  FastForwardAndExpect(BackgroundPolicy::kShortDelay,
                       {ReleaseUpdate(kShortId), ReleaseUpdate(kLateId)});
}

TEST_F(BackgroundPolicyTest, ConsumerAddedAfterDelayExpired) {
  Background();
  FastForwardAndExpect(BackgroundPolicy::kShortDelay,
                       {ReleaseUpdate(kShortId)});

  // A consumer added after its delay expired gets the background limit, but
  // isn't asked to release memory, since it had little or no time to grow.
  const std::string kLateName = "late";
  const uint32_t kLateId = base::PersistentHash(kLateName);
  ExpectUpdates({LimitUpdate(kLateId, BackgroundPolicy::kBackgroundLimit)},
                [&] { AddConsumer(kLateId, kLateName, kShortDelayTraits); });
}

TEST_F(BackgroundPolicyTest, OtherPolicyReleaseDuringDelay) {
  // Another policy that lowers the limit of all local consumers and asks them
  // to release memory, like the MemoryPressurePolicy under moderate pressure.
  PredicateMemoryCoordinatorPolicy other_policy(
      policy_manager_,
      base::BindRepeating([](uint32_t, std::string_view, Traits, ProcessType,
                             ChildProcessId child_process_id) {
        return child_process_id.is_null();
      }));
  std::optional<MemoryCoordinatorPolicyRegistration> other_registration(
      std::in_place, policy_manager_, other_policy);

  Background();

  // Limits are multiplied, so the consumers whose limit was lowered by the
  // BackgroundPolicy keep the background limit and release all their memory.
  constexpr base::MemoryLimit kOtherLimit =
      base::MemoryLimit::ModeratePressureThreshold();
  ExpectUpdates(
      {ReleaseUpdate(kShortId), ReleaseUpdate(kMediumId),
       ReleaseUpdate(kLongId),
       LimitAndReleaseUpdate(kNoBackgroundId, kOtherLimit)},
      [&] { other_policy.SetLimit(kOtherLimit, /*release_memory=*/true); });

  // The delay expiring still sends a release request.
  FastForwardAndExpect(BackgroundPolicy::kShortDelay,
                       {ReleaseUpdate(kShortId)});

  EXPECT_CALL(host_, UpdateConsumers(_)).Times(testing::AnyNumber());
  other_registration.reset();
  Mock::VerifyAndClearExpectations(&host_);
}

TEST_F(BackgroundPolicyTest, IgnoresConsumersInOtherProcesses) {
  // The browser's policy manager also tracks the consumers of child processes,
  // which have their own BackgroundPolicy.
  const ChildProcessId kChildProcessId(1);
  testing::StrictMock<MockMemoryConsumerGroupHost> child_host;
  policy_manager_.AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER,
                                             kChildProcessId, &child_host);
  const std::string kChildName = "child";
  const uint32_t kChildId = base::PersistentHash(kChildName);
  policy_manager_.OnConsumerGroupAdded(kChildId, kChildName, kShortDelayTraits,
                                       kChildProcessId);

  EXPECT_CALL(child_host, UpdateConsumers(_)).Times(0);
  Background();
  FastForwardAndExpect(BackgroundPolicy::kShortDelay,
                       {ReleaseUpdate(kShortId)});

  // A consumer added to a child process while backgrounded is ignored too.
  const std::string kLateChildName = "late_child";
  const uint32_t kLateChildId = base::PersistentHash(kLateChildName);
  policy_manager_.OnConsumerGroupAdded(kLateChildId, kLateChildName,
                                       kShortDelayTraits, kChildProcessId);
  Mock::VerifyAndClearExpectations(&child_host);

  policy_manager_.OnConsumerGroupRemoved(kLateChildId, kChildProcessId);
  policy_manager_.OnConsumerGroupRemoved(kChildId, kChildProcessId);
  policy_manager_.RemoveMemoryConsumerGroupHost(kChildProcessId);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(BackgroundPolicyTest, PreFreezeReleasesRemainingConsumers) {
  using base::android::PreFreezeBackgroundMemoryTrimmer;

  // Pre-freeze is only supported on Android 14+. Force it on so that the
  // release timers run early when the process is about to be frozen.
  const bool supported_modern_trim =
      PreFreezeBackgroundMemoryTrimmer::SupportsModernTrim();
  PreFreezeBackgroundMemoryTrimmer::SetSupportsModernTrimForTesting(true);
  PreFreezeBackgroundMemoryTrimmer::ResetForTesting();

  // The release timers pick their implementation when they are created, so the
  // policy must be recreated for them to run early on pre-freeze.
  policy_.reset();
  policy_.emplace(policy_manager_);

  Background();
  FastForwardAndExpect(BackgroundPolicy::kShortDelay,
                       {ReleaseUpdate(kShortId)});

  // The consumers whose delay hasn't expired yet are asked to release memory
  // right away. Each timer runs separately, so they are updated separately.
  EXPECT_CALL(host_, UpdateConsumers(ElementsAre(ReleaseUpdate(kMediumId))));
  EXPECT_CALL(host_, UpdateConsumers(ElementsAre(ReleaseUpdate(kLongId))));
  PreFreezeBackgroundMemoryTrimmer::OnPreFreezeForTesting();
  Mock::VerifyAndClearExpectations(&host_);

  // Nothing else happens.
  FastForwardAndExpect(BackgroundPolicy::kLongDelay, {});

  PreFreezeBackgroundMemoryTrimmer::ResetForTesting();
  PreFreezeBackgroundMemoryTrimmer::SetSupportsModernTrimForTesting(
      supported_modern_trim);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace content
