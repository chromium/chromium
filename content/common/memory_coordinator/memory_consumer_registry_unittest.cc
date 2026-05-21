// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/memory_coordinator/memory_consumer_registry.h"

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/check_op.h"
#include "base/hash/hash.h"
#include "base/memory/raw_ptr.h"
#include "base/memory_coordinator/mock_memory_consumer.h"
#include "base/memory_coordinator/traits.h"
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
  std::optional<base::MemoryConsumerTraits> traits;
  ProcessType process_type;
  ChildProcessId child_process_id;
  raw_ptr<MemoryConsumerGroupHost> host;
};

const std::optional<base::MemoryConsumerTraits> kTestTraits1 = std::nullopt;

}  // namespace

class MemoryConsumerRegistryTest : public Test,
                                   public MemoryConsumerGroupController {
 protected:
  MemoryConsumerRegistryTest()
      : registry_(PROCESS_TYPE_BROWSER, ChildProcessId(), *this) {}

  MemoryConsumerRegistry& registry() { return registry_.Get(); }

  std::vector<ConsumerEntry>& entries() { return entries_; }

  // MemoryConsumerGroupController:
  void AddMemoryConsumerGroupHost(ChildProcessId child_process_id,
                                  MemoryConsumerGroupHost* host) override {
    auto [_, inserted] = hosts_.try_emplace(child_process_id, host);
    CHECK(inserted);
  }

  void RemoveMemoryConsumerGroupHost(ChildProcessId child_process_id) override {
    size_t removed = hosts_.erase(child_process_id);
    CHECK_EQ(removed, 1u);
  }

  void OnConsumerGroupAdded(uint32_t consumer_id,
                            std::string_view consumer_name,
                            std::optional<base::MemoryConsumerTraits> traits,
                            ProcessType process_type,
                            ChildProcessId child_process_id) override {
    entries_.push_back({consumer_id, std::string(consumer_name), traits,
                        process_type, child_process_id,
                        hosts_.at(child_process_id)});
  }

  void OnConsumerGroupRemoved(uint32_t consumer_id,
                              ChildProcessId child_process_id) override {
    std::erase_if(entries_, [&](const auto& entry) {
      return entry.consumer_id == consumer_id &&
             entry.child_process_id == child_process_id;
    });
  }

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  void OnMemoryLimitChanged(uint32_t consumer_id,
                            ChildProcessId child_process_id,
                            int memory_limit) override {}
#endif

 private:
  base::test::TaskEnvironment task_environment_;
  std::map<ChildProcessId, MemoryConsumerGroupHost*> hosts_;
  base::ScopedMemoryConsumerRegistry<MemoryConsumerRegistry> registry_;
  std::vector<ConsumerEntry> entries_;
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
  entries().front().host->UpdateConsumers({{kConsumerId, std::nullopt, true}});
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

  const int kNewLimit = 50;
  EXPECT_CALL(consumer1, OnUpdateMemoryLimit());
  entries().front().host->UpdateConsumers({{kConsumerId, kNewLimit, false}});
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
  entries().front().host->UpdateConsumers({{kConsumerId, std::nullopt, true}});

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
  // consumer. Since it was the last consumer, the group is destroyed. Then it
  // should NOT crash when attempting to call ReleaseMemory on the destroyed
  // group.
  entries().front().host->UpdateConsumers({{kConsumerId, 50, true}});

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
  entries().front().host->UpdateConsumers({{kConsumerId, 50, false}});

  // Verify it was called and successfully removed itself without crashing!
  EXPECT_TRUE(consumer.limit_updated());
  ASSERT_EQ(registry().size(), 0u);
}

}  // namespace content
