// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/memory_coordinator/memory_consumer_registry.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/check_op.h"
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
  std::string consumer_id;
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

  void OnConsumerGroupAdded(std::string_view consumer_id,
                            std::optional<base::MemoryConsumerTraits> traits,
                            ProcessType process_type,
                            ChildProcessId child_process_id) override {
    entries_.push_back({std::string(consumer_id), traits, process_type,
                        child_process_id, hosts_.at(child_process_id)});
  }

  void OnConsumerGroupRemoved(std::string_view consumer_id,
                              ChildProcessId child_process_id) override {
    std::erase_if(entries_, [&](const auto& entry) {
      return entry.consumer_id == consumer_id &&
             entry.child_process_id == child_process_id;
    });
  }

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  void OnMemoryLimitChanged(std::string_view consumer_id,
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

  registry().AddMemoryConsumer("consumer", kTestTraits1, &consumer);
  ASSERT_EQ(registry().size(), 1u);
  ASSERT_EQ(entries().size(), 1u);

  // Verify group creation notification
  EXPECT_EQ(entries().front().consumer_id, "consumer");
  EXPECT_EQ(entries().front().process_type, PROCESS_TYPE_BROWSER);

  // Release memory propagation
  EXPECT_CALL(consumer, OnReleaseMemory());
  entries().front().host->UpdateConsumers(
      {{std::string("consumer"), std::nullopt, true}});
  Mock::VerifyAndClearExpectations(&consumer);

  registry().RemoveMemoryConsumer("consumer", &consumer);
  ASSERT_EQ(registry().size(), 0u);
  ASSERT_EQ(entries().size(), 0u);
}

TEST_F(MemoryConsumerRegistryTest, InheritMemoryLimit) {
  base::MockMemoryConsumer consumer1;
  base::MockMemoryConsumer consumer2;

  registry().AddMemoryConsumer("consumer", kTestTraits1, &consumer1);

  const int kNewLimit = 50;
  EXPECT_CALL(consumer1, OnUpdateMemoryLimit());
  entries().front().host->UpdateConsumers(
      {{std::string("consumer"), kNewLimit, false}});
  EXPECT_EQ(consumer1.memory_limit(), kNewLimit);

  // New consumer should inherit limit
  EXPECT_CALL(consumer2, OnUpdateMemoryLimit());
  registry().AddMemoryConsumer("consumer", kTestTraits1, &consumer2);
  EXPECT_EQ(consumer2.memory_limit(), kNewLimit);

  registry().RemoveMemoryConsumer("consumer", &consumer1);
  registry().RemoveMemoryConsumer("consumer", &consumer2);
}

}  // namespace content
