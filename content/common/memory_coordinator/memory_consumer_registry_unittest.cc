// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/memory_coordinator/memory_consumer_registry.h"

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/hash/hash.h"
#include "base/memory/raw_ptr.h"
#include "base/memory_coordinator/mock_memory_consumer.h"
#include "base/memory_coordinator/traits.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "content/common/buildflags.h"
#include "content/common/memory_coordinator/memory_consumer_group_controller.h"
#include "content/common/memory_coordinator/memory_consumer_group_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using ::testing::Mock;
using ::testing::Test;

struct ConsumerEntry {
  uint32_t consumer_id;
  std::string consumer_name;
  base::MemoryConsumerTraits traits;
  ProcessType process_type;
  ChildProcessId child_process_id;
  raw_ptr<MemoryConsumerGroupHost> host;
};

constexpr base::MemoryConsumerTraits kTestTraits1(
    base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
    base::MemoryConsumerTraits::ReleaseMemoryCost::kRequiresTraversal,
    base::MemoryConsumerTraits::InformationRetention::kLossless,
    base::MemoryConsumerTraits::ExecutionType::kSynchronous);

}  // namespace

class MemoryConsumerRegistryTest : public Test,
                                   public MemoryConsumerGroupController {
 protected:
  using OnConsumerGroupAddedCallback =
      base::RepeatingCallback<void(uint32_t consumer_id,
                                   MemoryConsumerGroupHost* host)>;

  MemoryConsumerRegistryTest()
      : registry_(PROCESS_TYPE_BROWSER, ChildProcessId(), *this) {}

  MemoryConsumerRegistry& registry() { return registry_.Get(); }

  std::vector<ConsumerEntry>& entries() { return entries_; }

  // Invoked from OnConsumerGroupAdded(), after the entry is recorded. Lets a
  // test mimic a policy that synchronously pushes an initial limit.
  void set_on_consumer_group_added(OnConsumerGroupAddedCallback callback) {
    on_consumer_group_added_ = std::move(callback);
  }

  size_t removed_group_count() const { return removed_group_count_; }

  // MemoryConsumerGroupController:
  void AddMemoryConsumerGroupHost(ProcessType process_type,
                                  ChildProcessId child_process_id,
                                  MemoryConsumerGroupHost* host) override {
    auto [_, inserted] =
        hosts_.try_emplace(child_process_id, HostInfo{host, process_type});
    CHECK(inserted);
  }

  void RemoveMemoryConsumerGroupHost(ChildProcessId child_process_id) override {
    size_t removed = hosts_.erase(child_process_id);
    CHECK_EQ(removed, 1u);
  }

  void OnConsumerGroupAdded(uint32_t consumer_id,
                            std::string_view consumer_name,
                            base::MemoryConsumerTraits traits,
                            ChildProcessId child_process_id) override {
    const HostInfo& host_info = hosts_.at(child_process_id);
    entries_.push_back({consumer_id, std::string(consumer_name), traits,
                        host_info.process_type, child_process_id,
                        host_info.host});
    if (on_consumer_group_added_) {
      on_consumer_group_added_.Run(consumer_id, host_info.host);
    }
  }

  void OnConsumerGroupRemoved(uint32_t consumer_id,
                              ChildProcessId child_process_id) override {
    std::erase_if(entries_, [&](const auto& entry) {
      return entry.consumer_id == consumer_id &&
             entry.child_process_id == child_process_id;
    });
    ++removed_group_count_;
  }

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  void OnMemoryLimitChanged(uint32_t consumer_id,
                            ChildProcessId child_process_id,
                            base::MemoryLimit memory_limit) override {}
#endif

 private:
  base::test::TaskEnvironment task_environment_;
  struct HostInfo {
    raw_ptr<MemoryConsumerGroupHost> host;
    ProcessType process_type;
  };
  std::map<ChildProcessId, HostInfo> hosts_;
  base::ScopedMemoryConsumerRegistry<MemoryConsumerRegistry> registry_;
  std::vector<ConsumerEntry> entries_;
  OnConsumerGroupAddedCallback on_consumer_group_added_;
  size_t removed_group_count_ = 0;
};

// A consumer that runs a caller-provided closure (at most once) when asked to
// release memory. Used to exercise re-entrant registry calls from callbacks.
class ClosureOnReleaseMemoryConsumer : public base::MemoryConsumer {
 public:
  explicit ClosureOnReleaseMemoryConsumer(
      base::OnceClosure on_release_memory = base::OnceClosure())
      : on_release_memory_(std::move(on_release_memory)) {}

  ~ClosureOnReleaseMemoryConsumer() override = default;

  // base::MemoryConsumer:
  void OnReleaseMemory() override {
    if (on_release_memory_) {
      std::move(on_release_memory_).Run();
    }
  }
  void OnUpdateMemoryLimit() override {}

 private:
  base::OnceClosure on_release_memory_;
};

TEST_F(MemoryConsumerRegistryTest, AddRemoveConsumer) {
  base::MockMemoryConsumer consumer;
  const std::string kConsumerName = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  registry().AddMemoryConsumer(kConsumerName, kTestTraits1, &consumer);
  ASSERT_EQ(registry().size(), 1u);
  ASSERT_EQ(entries().size(), 1u);

  // Verify group creation notification
  EXPECT_EQ(entries().front().consumer_name, kConsumerName);
  EXPECT_EQ(entries().front().consumer_id, kConsumerId);
  EXPECT_EQ(entries().front().process_type, PROCESS_TYPE_BROWSER);

  // Release memory propagation
  EXPECT_CALL(consumer, OnReleaseMemory());
  entries().front().host->UpdateConsumers(
      {{kConsumerId, std::nullopt, /*release_memory=*/true}});
  Mock::VerifyAndClearExpectations(&consumer);

  registry().RemoveMemoryConsumer(kConsumerName, &consumer);
  ASSERT_EQ(registry().size(), 0u);
  ASSERT_EQ(entries().size(), 0u);
}

TEST_F(MemoryConsumerRegistryTest, InheritMemoryLimit) {
  base::MockMemoryConsumer consumer1;
  base::MockMemoryConsumer consumer2;
  const std::string kConsumerName = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  registry().AddMemoryConsumer(kConsumerName, kTestTraits1, &consumer1);

  constexpr base::MemoryLimit kNewLimit = base::MemoryLimit::FromPercent(50);
  EXPECT_CALL(consumer1, OnUpdateMemoryLimit());
  entries().front().host->UpdateConsumers(
      {{kConsumerId, kNewLimit, /*release_memory=*/false}});
  EXPECT_EQ(consumer1.memory_limit(), kNewLimit);

  // New consumer should inherit limit without calling OnUpdateMemoryLimit
  EXPECT_CALL(consumer2, OnUpdateMemoryLimit()).Times(0);
  registry().AddMemoryConsumer(kConsumerName, kTestTraits1, &consumer2);
  EXPECT_EQ(consumer2.memory_limit(), kNewLimit);

  registry().RemoveMemoryConsumer(kConsumerName, &consumer1);
  registry().RemoveMemoryConsumer(kConsumerName, &consumer2);
}

class ReentrantSelfRemovingMemoryConsumer : public base::MemoryConsumer {
 public:
  ReentrantSelfRemovingMemoryConsumer(MemoryConsumerRegistry& registry,
                                      const std::string& name)
      : registry_(registry), name_(name) {}

  ~ReentrantSelfRemovingMemoryConsumer() override = default;

  void OnReleaseMemory() override {
    registry_->RemoveMemoryConsumer(name_, this);
    released_ = true;
  }

  void OnUpdateMemoryLimit() override {}

  bool released() const { return released_; }

 private:
  const raw_ref<MemoryConsumerRegistry> registry_;
  std::string name_;
  bool released_ = false;
};

TEST_F(MemoryConsumerRegistryTest, ReentrantRemoval) {
  const std::string kConsumerName = "reentrant_consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  ReentrantSelfRemovingMemoryConsumer consumer(registry(), kConsumerName);

  registry().AddMemoryConsumer(kConsumerName, kTestTraits1, &consumer);
  ASSERT_EQ(registry().size(), 1u);

  // Trigger release memory, which will call OnReleaseMemory and cause the
  // consumer to remove itself.
  entries().front().host->UpdateConsumers(
      {{kConsumerId, std::nullopt, /*release_memory=*/true}});

  // Verify it was called and successfully removed itself without crashing!
  EXPECT_TRUE(consumer.released());
  ASSERT_EQ(registry().size(), 0u);
}

class ReentrantSelfRemovingOnLimitMemoryConsumer : public base::MemoryConsumer {
 public:
  ReentrantSelfRemovingOnLimitMemoryConsumer(MemoryConsumerRegistry& registry,
                                             const std::string& name)
      : registry_(registry), name_(name) {}

  ~ReentrantSelfRemovingOnLimitMemoryConsumer() override = default;

  void OnReleaseMemory() override { released_ = true; }

  void OnUpdateMemoryLimit() override {
    registry_->RemoveMemoryConsumer(name_, this);
    limit_updated_ = true;
  }

  bool released() const { return released_; }
  bool limit_updated() const { return limit_updated_; }

 private:
  const raw_ref<MemoryConsumerRegistry> registry_;
  std::string name_;
  bool released_ = false;
  bool limit_updated_ = false;
};

TEST_F(MemoryConsumerRegistryTest, ReentrantRemovalDuringLimitUpdate) {
  const std::string kConsumerName = "reentrant_limit_consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  ReentrantSelfRemovingOnLimitMemoryConsumer consumer(registry(),
                                                      kConsumerName);

  registry().AddMemoryConsumer(kConsumerName, kTestTraits1, &consumer);
  ASSERT_EQ(registry().size(), 1u);

  // Trigger update with both limit and release.
  // The limit update should trigger OnUpdateMemoryLimit, which removes the
  // consumer. Since it was the last consumer, the group becomes empty but must
  // stay alive until the batch completes: the subsequent ReleaseMemory() is a
  // no-op, and the group is destroyed afterwards.
  entries().front().host->UpdateConsumers(
      {{kConsumerId, base::MemoryLimit::FromPercent(50),
        /*release_memory=*/true}});

  // Verify it was called and successfully removed itself without crashing!
  EXPECT_TRUE(consumer.limit_updated());
  EXPECT_FALSE(consumer.released());
  ASSERT_EQ(registry().size(), 0u);
}

TEST_F(MemoryConsumerRegistryTest, ReentrantRemovalDuringLimitUpdateOnly) {
  const std::string kConsumerName = "reentrant_limit_only_consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  ReentrantSelfRemovingOnLimitMemoryConsumer consumer(registry(),
                                                      kConsumerName);

  registry().AddMemoryConsumer(kConsumerName, kTestTraits1, &consumer);
  ASSERT_EQ(registry().size(), 1u);

  // Trigger update with limit ONLY.
  entries().front().host->UpdateConsumers(
      {{kConsumerId, base::MemoryLimit::FromPercent(50),
        /*release_memory=*/false}});

  // Verify it was called and successfully removed itself without crashing!
  EXPECT_TRUE(consumer.limit_updated());
  ASSERT_EQ(registry().size(), 0u);
}

TEST_F(MemoryConsumerRegistryTest, ReentrantRemovalDuringOverrideLimit) {
  const std::string kConsumerName = "reentrant_override_consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  ReentrantSelfRemovingOnLimitMemoryConsumer consumer(registry(),
                                                      kConsumerName);

  registry().AddMemoryConsumer(kConsumerName, kTestTraits1, &consumer);
  ASSERT_EQ(registry().size(), 1u);

  // Trigger override limit update.
  entries().front().host->SetOverrideLimit(kConsumerId,
                                           base::MemoryLimit::FromPercent(50));

  EXPECT_TRUE(consumer.limit_updated());
  ASSERT_EQ(registry().size(), 0u);
}

TEST_F(MemoryConsumerRegistryTest, ReentrantRemovalDuringClearOverrideLimit) {
  const std::string kConsumerName = "reentrant_clear_override_consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  ReentrantSelfRemovingOnLimitMemoryConsumer consumer(registry(),
                                                      kConsumerName);

  registry().AddMemoryConsumer(kConsumerName, kTestTraits1, &consumer);
  ASSERT_EQ(registry().size(), 1u);

  // Trigger clear override limit update.
  entries().front().host->ClearOverrideLimit(kConsumerId,
                                             base::MemoryLimit::Default());

  EXPECT_TRUE(consumer.limit_updated());
  ASSERT_EQ(registry().size(), 0u);
}

// A policy may synchronously push an initial limit to a group as soon as it
// is created. The consumer must receive the limit without being notified
// (it is still being registered), and the group must survive.
TEST_F(MemoryConsumerRegistryTest, InitialLimitPushedDuringRegistration) {
  constexpr base::MemoryLimit kInitialLimit =
      base::MemoryLimit::FromPercent(50);
  set_on_consumer_group_added(base::BindLambdaForTesting(
      [&](uint32_t consumer_id, MemoryConsumerGroupHost* host) {
        host->UpdateConsumers(
            {{consumer_id, kInitialLimit, /*release_memory=*/false}});
      }));

  base::MockMemoryConsumer consumer;
  const std::string kConsumerName = "consumer";

  EXPECT_CALL(consumer, OnUpdateMemoryLimit()).Times(0);
  registry().AddMemoryConsumer(kConsumerName, kTestTraits1, &consumer);
  EXPECT_EQ(consumer.memory_limit(), kInitialLimit);
  ASSERT_EQ(registry().size(), 1u);
  ASSERT_EQ(entries().size(), 1u);

  registry().RemoveMemoryConsumer(kConsumerName, &consumer);
  ASSERT_EQ(registry().size(), 0u);
}

// Releasing memory for group A causes a consumer with a new ID (B) to be
// registered, and the policy synchronously pushes an initial limit to B. This
// re-enters UpdateConsumers() for B while A's batch is in flight, which must
// be allowed.
TEST_F(MemoryConsumerRegistryTest, ReentrantUpdateOfNewGroupDuringRelease) {
  constexpr base::MemoryLimit kInitialLimit =
      base::MemoryLimit::FromPercent(50);
  set_on_consumer_group_added(base::BindLambdaForTesting(
      [&](uint32_t consumer_id, MemoryConsumerGroupHost* host) {
        host->UpdateConsumers(
            {{consumer_id, kInitialLimit, /*release_memory=*/false}});
      }));

  const std::string kConsumerNameA = "consumer_a";
  const std::string kConsumerNameB = "consumer_b";
  const uint32_t kConsumerIdA = base::PersistentHash(kConsumerNameA);

  base::MockMemoryConsumer consumer_b;
  ClosureOnReleaseMemoryConsumer consumer_a(base::BindLambdaForTesting([&] {
    registry().AddMemoryConsumer(kConsumerNameB, kTestTraits1, &consumer_b);
  }));

  registry().AddMemoryConsumer(kConsumerNameA, kTestTraits1, &consumer_a);
  ASSERT_EQ(registry().size(), 1u);

  EXPECT_CALL(consumer_b, OnUpdateMemoryLimit()).Times(0);
  entries().front().host->UpdateConsumers(
      {{kConsumerIdA, std::nullopt, /*release_memory=*/true}});

  EXPECT_EQ(registry().size(), 2u);
  EXPECT_EQ(entries().size(), 2u);
  EXPECT_EQ(consumer_b.memory_limit(), kInitialLimit);

  registry().RemoveMemoryConsumer(kConsumerNameB, &consumer_b);
  registry().RemoveMemoryConsumer(kConsumerNameA, &consumer_a);
  ASSERT_EQ(registry().size(), 0u);
}

// The last consumer of a group unregisters during an update, and another
// consumer registers under the same ID before the batch completes. The group
// must be kept, and the controller must not observe a removal.
TEST_F(MemoryConsumerRegistryTest, ReAddDuringUpdateKeepsGroupAlive) {
  const std::string kConsumerName = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);
  constexpr base::MemoryLimit kNewLimit = base::MemoryLimit::FromPercent(50);

  base::MockMemoryConsumer consumer2;
  ClosureOnReleaseMemoryConsumer consumer1(base::BindLambdaForTesting([&] {
    registry().RemoveMemoryConsumer(kConsumerName, &consumer1);
    registry().AddMemoryConsumer(kConsumerName, kTestTraits1, &consumer2);
  }));

  registry().AddMemoryConsumer(kConsumerName, kTestTraits1, &consumer1);
  ASSERT_EQ(registry().size(), 1u);

  // The re-added consumer is not notified by the in-flight batch.
  EXPECT_CALL(consumer2, OnReleaseMemory()).Times(0);
  EXPECT_CALL(consumer2, OnUpdateMemoryLimit()).Times(0);
  entries().front().host->UpdateConsumers(
      {{kConsumerId, kNewLimit, /*release_memory=*/true}});

  EXPECT_EQ(registry().size(), 1u);
  EXPECT_EQ(entries().size(), 1u);
  EXPECT_EQ(removed_group_count(), 0u);
  // The re-added consumer inherits the group's limit.
  EXPECT_EQ(consumer2.memory_limit(), kNewLimit);

  registry().RemoveMemoryConsumer(kConsumerName, &consumer2);
  EXPECT_EQ(registry().size(), 0u);
  EXPECT_EQ(removed_group_count(), 1u);
}

// Re-entering UpdateConsumers() for a group that is currently being updated is
// a programming error.
TEST_F(MemoryConsumerRegistryTest, ReentrantUpdateOfSameGroupChecks) {
  const std::string kConsumerName = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  ClosureOnReleaseMemoryConsumer consumer(base::BindLambdaForTesting([&] {
    entries().front().host->UpdateConsumers(
        {{kConsumerId, std::nullopt, /*release_memory=*/true}});
  }));
  registry().AddMemoryConsumer(kConsumerName, kTestTraits1, &consumer);

  EXPECT_CHECK_DEATH(entries().front().host->UpdateConsumers(
      {{kConsumerId, std::nullopt, /*release_memory=*/true}}));

  registry().RemoveMemoryConsumer(kConsumerName, &consumer);
}

// Same as above, but the re-entrant update targets a group that is later in
// the same batch and has not been reached yet. It is still part of the
// in-flight batch and must be rejected.
TEST_F(MemoryConsumerRegistryTest, ReentrantUpdateOfGroupLaterInBatchChecks) {
  const std::string kConsumerNameA = "consumer_a";
  const std::string kConsumerNameB = "consumer_b";
  const uint32_t kConsumerIdA = base::PersistentHash(kConsumerNameA);
  const uint32_t kConsumerIdB = base::PersistentHash(kConsumerNameB);

  ClosureOnReleaseMemoryConsumer consumer_a(base::BindLambdaForTesting([&] {
    entries().front().host->UpdateConsumers(
        {{kConsumerIdB, std::nullopt, /*release_memory=*/true}});
  }));
  ClosureOnReleaseMemoryConsumer consumer_b;
  registry().AddMemoryConsumer(kConsumerNameA, kTestTraits1, &consumer_a);
  registry().AddMemoryConsumer(kConsumerNameB, kTestTraits1, &consumer_b);

  EXPECT_CHECK_DEATH(entries().front().host->UpdateConsumers(
      {{kConsumerIdA, std::nullopt, /*release_memory=*/true},
       {kConsumerIdB, std::nullopt, /*release_memory=*/true}}));

  registry().RemoveMemoryConsumer(kConsumerNameB, &consumer_b);
  registry().RemoveMemoryConsumer(kConsumerNameA, &consumer_a);
}

}  // namespace content
