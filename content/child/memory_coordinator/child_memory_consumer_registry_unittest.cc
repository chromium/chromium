// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/memory_coordinator/child_memory_consumer_registry.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory_coordinator/mock_memory_consumer.h"
#include "base/memory_coordinator/traits.h"
#include "base/test/task_environment.h"
#include "content/common/memory_coordinator/memory_consumer_group_controller.h"
#include "content/common/memory_coordinator/memory_consumer_group_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using ::testing::Mock;
using ::testing::NiceMock;
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

class ChildMemoryConsumerRegistryTest : public Test,
                                        public MemoryConsumerGroupController {
 protected:
  ChildMemoryConsumerRegistryTest() : registry_(*this) {}

  ChildMemoryConsumerRegistry& registry() { return registry_.Get(); }

  std::vector<ConsumerEntry>& entries() { return entries_; }

  // MemoryConsumerGroupController:
  void AddMemoryConsumerGroupHost(ChildProcessId child_process_id,
                                  MemoryConsumerGroupHost* host) override {
    hosts_[child_process_id] = host;
  }

  void RemoveMemoryConsumerGroupHost(ChildProcessId child_process_id) override {
    hosts_.erase(child_process_id);
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

 private:
  base::test::TaskEnvironment task_environment_;
  std::map<ChildProcessId, MemoryConsumerGroupHost*> hosts_;
  base::ScopedMemoryConsumerRegistry<ChildMemoryConsumerRegistry> registry_;
  std::vector<ConsumerEntry> entries_;
};

TEST_F(ChildMemoryConsumerRegistryTest, AddRemoveConsumer) {
  base::MockMemoryConsumer consumer;

  registry().AddMemoryConsumer("consumer", kTestTraits1, &consumer);
  ASSERT_EQ(registry().size(), 1u);
  ASSERT_EQ(entries().size(), 1u);

  // Verify group creation notification
  EXPECT_EQ(entries().front().consumer_id, "consumer");
  EXPECT_EQ(entries().front().process_type, PROCESS_TYPE_UNKNOWN);

  // Release memory propagation
  EXPECT_CALL(consumer, OnReleaseMemory());
  entries().front().host->ReleaseMemory("consumer");
  Mock::VerifyAndClearExpectations(&consumer);

  registry().RemoveMemoryConsumer("consumer", &consumer);
  ASSERT_EQ(registry().size(), 0u);
  ASSERT_EQ(entries().size(), 0u);
}

TEST_F(ChildMemoryConsumerRegistryTest, InheritMemoryLimit) {
  base::MockMemoryConsumer consumer1;
  base::MockMemoryConsumer consumer2;

  registry().AddMemoryConsumer("consumer", kTestTraits1, &consumer1);

  const int kNewLimit = 50;
  EXPECT_CALL(consumer1, OnUpdateMemoryLimit());
  entries().front().host->UpdateMemoryLimit("consumer", kNewLimit);
  EXPECT_EQ(consumer1.memory_limit(), kNewLimit);

  // New consumer should inherit limit
  EXPECT_CALL(consumer2, OnUpdateMemoryLimit());
  registry().AddMemoryConsumer("consumer", kTestTraits1, &consumer2);
  EXPECT_EQ(consumer2.memory_limit(), kNewLimit);

  registry().RemoveMemoryConsumer("consumer", &consumer1);
  registry().RemoveMemoryConsumer("consumer", &consumer2);
}

}  // namespace content
