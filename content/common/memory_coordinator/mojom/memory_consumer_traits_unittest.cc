// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/memory_coordinator/traits.h"
#include "base/test/task_environment.h"
#include "content/common/memory_coordinator/mojom/memory_consumer_traits_test.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using SupportsMemoryLimit = base::MemoryConsumerTraits::SupportsMemoryLimit;
using InProcess = base::MemoryConsumerTraits::InProcess;
using EstimatedMemoryUsage = base::MemoryConsumerTraits::EstimatedMemoryUsage;
using ReleaseMemoryCost = base::MemoryConsumerTraits::ReleaseMemoryCost;
using RecreateMemoryCost = base::MemoryConsumerTraits::RecreateMemoryCost;
using InformationRetention = base::MemoryConsumerTraits::InformationRetention;
using MemoryReleaseBehavior = base::MemoryConsumerTraits::MemoryReleaseBehavior;
using ExecutionType = base::MemoryConsumerTraits::ExecutionType;
using ReleaseGCReferences = base::MemoryConsumerTraits::ReleaseGCReferences;
using GarbageCollectsV8Heap = base::MemoryConsumerTraits::GarbageCollectsV8Heap;

class MemoryConsumerTraitsTest : public testing::Test,
                                 public mojom::MemoryConsumerTraitsTest {
 protected:
  MemoryConsumerTraitsTest()
      : receiver_{this, remote_.BindNewPipeAndPassReceiver()} {}

  // mojom::MemoryConsumerTraitsTest:
  void EchoMemoryConsumerTraits(
      base::MemoryConsumerTraits traits,
      EchoMemoryConsumerTraitsCallback callback) override {
    std::move(callback).Run(traits);
  }

  const mojo::Remote<mojom::MemoryConsumerTraitsTest>& remote() const {
    return remote_;
  }

 private:
  base::test::TaskEnvironment task_environment_;

  mojo::Remote<mojom::MemoryConsumerTraitsTest> remote_;
  mojo::Receiver<mojom::MemoryConsumerTraitsTest> receiver_{this};
};

}  // namespace

TEST_F(MemoryConsumerTraitsTest, EchoAllTraits) {
  static constexpr base::MemoryConsumerTraits kTestCases[] = {
      {
          .supports_memory_limit = SupportsMemoryLimit::kNo,
          .in_process = InProcess::kNo,
          .estimated_memory_usage = EstimatedMemoryUsage::kSmall,
          .release_memory_cost = ReleaseMemoryCost::kFreesPagesWithoutTraversal,
          .recreate_memory_cost = RecreateMemoryCost::kNA,
          .information_retention = InformationRetention::kLossy,
          .memory_release_behavior = MemoryReleaseBehavior::kRepeatable,
          .execution_type = ExecutionType::kSynchronous,
          .release_gc_references = ReleaseGCReferences::kYes,
          .garbage_collects_v8_heap = GarbageCollectsV8Heap::kYes,
      },
      {
          .supports_memory_limit = SupportsMemoryLimit::kYes,
          .in_process = InProcess::kYes,
          .estimated_memory_usage = EstimatedMemoryUsage::kMedium,
          .release_memory_cost = ReleaseMemoryCost::kRequiresTraversal,
          .recreate_memory_cost = RecreateMemoryCost::kCheap,
          .information_retention = InformationRetention::kLossless,
          .memory_release_behavior = MemoryReleaseBehavior::kIdempotent,
          .execution_type = ExecutionType::kAsynchronous,
          .release_gc_references = ReleaseGCReferences::kNo,
          .garbage_collects_v8_heap = GarbageCollectsV8Heap::kNo,
      },
      {
          .estimated_memory_usage = EstimatedMemoryUsage::kLarge,
          .recreate_memory_cost = RecreateMemoryCost::kExpensive,
      },
  };

  for (const auto& test_case : kTestCases) {
    base::MemoryConsumerTraits out = {};
    ASSERT_TRUE(remote()->EchoMemoryConsumerTraits(test_case, &out));
    EXPECT_EQ(test_case, out);
  }
}

TEST_F(MemoryConsumerTraitsTest, OutOfRange) {
  static constexpr base::MemoryConsumerTraits kTestCase = {
      .supports_memory_limit = static_cast<SupportsMemoryLimit>(22),
  };

  base::MemoryConsumerTraits out = {};
  EXPECT_FALSE(remote()->EchoMemoryConsumerTraits(kTestCase, &out));
  EXPECT_NE(kTestCase, out);
}

}  // namespace content
