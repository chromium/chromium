// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/memory_coordinator/child_memory_consumer_registry.h"

#include <string>
#include <vector>

#include "base/memory_coordinator/mock_memory_consumer.h"
#include "base/memory_coordinator/traits.h"
#include "base/test/task_environment.h"
#include "content/common/memory_coordinator/memory_consumer_group_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using ::testing::Mock;
using ::testing::Test;

struct ConsumerEntry {
  std::string consumer_id;
  base::MemoryConsumerTraits traits;
  ProcessType process_type;
  ChildProcessId child_process_id;
  base::RegisteredMemoryConsumer consumer;
};

const base::MemoryConsumerTraits kTestTraits1{};

}  // namespace

class ChildMemoryConsumerRegistryTest : public Test,
                                        public MemoryConsumerGroupController {
 protected:
  ChildMemoryConsumerRegistryTest() : registry_(*this) {}

  ChildMemoryConsumerRegistry& registry() { return registry_; }

  std::vector<ConsumerEntry>& entries() { return entries_; }

  // MemoryConsumerGroupController:
  void OnConsumerGroupAdded(std::string_view consumer_id,
                            base::MemoryConsumerTraits traits,
                            ProcessType process_type,
                            ChildProcessId child_process_id,
                            base::RegisteredMemoryConsumer consumer) override {
    entries_.push_back({std::string(consumer_id), traits, process_type,
                        child_process_id, consumer});
  }

  void OnConsumerGroupRemoved(std::string_view consumer_id,
                              ChildProcessId child_process_id) override {
    std::erase_if(entries_, [&](const auto& entry) {
      return entry.consumer_id == consumer_id &&
             entry.child_process_id == child_process_id;
    });
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  ChildMemoryConsumerRegistry registry_;
  std::vector<ConsumerEntry> entries_;
};

TEST_F(ChildMemoryConsumerRegistryTest, LocalConsumer) {
  base::MockMemoryConsumer consumer;

  // Add the consumer.
  registry().AddMemoryConsumer("consumer", kTestTraits1, &consumer);
  ASSERT_EQ(registry().size(), 1u);
  ASSERT_EQ(entries().size(), 1u);

  // Notify the consumer through the group controller's captured consumer.
  EXPECT_CALL(consumer, OnReleaseMemory());
  entries().front().consumer.ReleaseMemory();
  Mock::VerifyAndClearExpectations(&consumer);

  // Remove the consumer.
  registry().RemoveMemoryConsumer("consumer", &consumer);
  ASSERT_EQ(registry().size(), 0u);
  ASSERT_EQ(entries().size(), 0u);
}

TEST_F(ChildMemoryConsumerRegistryTest, InheritMemoryLimit) {
  base::MockMemoryConsumer consumer1;
  base::MockMemoryConsumer consumer2;

  // Add the first consumer.
  registry().AddMemoryConsumer("consumer", kTestTraits1, &consumer1);
  ASSERT_EQ(registry().size(), 1u);
  ASSERT_EQ(entries().size(), 1u);

  // Update the memory limit of the group.
  const int kNewLimit = 50;
  EXPECT_CALL(consumer1, OnUpdateMemoryLimit());
  entries().front().consumer.UpdateMemoryLimit(kNewLimit);
  ASSERT_EQ(consumer1.memory_limit(), kNewLimit);

  // Add the second consumer of the same group.
  // It should immediately inherit the memory limit of 50.
  EXPECT_CALL(consumer2, OnUpdateMemoryLimit());
  registry().AddMemoryConsumer("consumer", kTestTraits1, &consumer2);

  EXPECT_EQ(consumer2.memory_limit(), kNewLimit);

  // Cleanup.
  registry().RemoveMemoryConsumer("consumer", &consumer1);
  registry().RemoveMemoryConsumer("consumer", &consumer2);
}

}  // namespace content
