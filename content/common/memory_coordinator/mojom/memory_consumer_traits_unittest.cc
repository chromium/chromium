// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <utility>

#include "base/memory_coordinator/traits.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/common/memory_coordinator/mojom/memory_consumer_traits_test.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using EstimatedMemoryUsage = base::MemoryConsumerTraits::EstimatedMemoryUsage;
using ReleaseMemoryCost = base::MemoryConsumerTraits::ReleaseMemoryCost;
using InformationRetention = base::MemoryConsumerTraits::InformationRetention;
using ExecutionType = base::MemoryConsumerTraits::ExecutionType;
using SupportsMemoryLimit = base::MemoryConsumerTraits::SupportsMemoryLimit;
using InProcess = base::MemoryConsumerTraits::InProcess;
using RecreateMemoryCost = base::MemoryConsumerTraits::RecreateMemoryCost;
using MemoryReleaseBehavior = base::MemoryConsumerTraits::MemoryReleaseBehavior;
using ReleaseGCReferences = base::MemoryConsumerTraits::ReleaseGCReferences;
using GarbageCollectsV8Heap = base::MemoryConsumerTraits::GarbageCollectsV8Heap;
using IsStateful = base::MemoryConsumerTraits::IsStateful;

// Helper to determine the maximum value among all trait enums in a
// ParameterPack.
template <typename... Enums>
consteval int GetMaxValue(base::ParameterPack<Enums...>) {
  return std::max({static_cast<int>(Enums::kMaxValue)...});
}

// Helper to generate a test value for a trait enum based on a loop index.
// This ensures that every possible enum value (from 0 to kMaxValue) is
// produced at least once across the test.
template <typename T>
constexpr T GenerateValue(int index) {
  // Convert the index to the value.
  T value = static_cast<T>(index);

  // If the index exceeds the number of values for this specific trait, we
  // clamp it to kMaxValue, to ensure we still use a valid value.
  if (value > T::kMaxValue) {
    value = T::kMaxValue;
  }

  return value;
}

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

  void SetDisconnectHandler(base::OnceClosure disconnect_handler) {
    remote_.set_disconnect_handler(std::move(disconnect_handler));
  }

 private:
  base::test::TaskEnvironment task_environment_;

  mojo::Remote<mojom::MemoryConsumerTraitsTest> remote_;
  mojo::Receiver<mojom::MemoryConsumerTraitsTest> receiver_{this};
};

}  // namespace

// Tests that every possible trait value can be serialized and deserialized.
TEST_F(MemoryConsumerTraitsTest, EchoAllTraits) {
  // The maximum possible value across all trait enums.
  const int kMaxValue =
      GetMaxValue(base::MemoryConsumerTraits::AllTraitsList{});

  // By iterating up to this maximum, we guarantee that every single enum value
  // for every trait is serialized and deserialized at least once in a minimal
  // amount of mojo calls.
  for (int i = 0; i <= kMaxValue; ++i) {
    base::MemoryConsumerTraits test_case(
        GenerateValue<EstimatedMemoryUsage>(i),
        GenerateValue<ReleaseMemoryCost>(i),
        GenerateValue<InformationRetention>(i), GenerateValue<ExecutionType>(i),
        GenerateValue<SupportsMemoryLimit>(i), GenerateValue<InProcess>(i),
        GenerateValue<RecreateMemoryCost>(i),
        GenerateValue<MemoryReleaseBehavior>(i),
        GenerateValue<ReleaseGCReferences>(i),
        GenerateValue<GarbageCollectsV8Heap>(i), GenerateValue<IsStateful>(i));

    base::test::TestFuture<base::MemoryConsumerTraits> future;
    remote()->EchoMemoryConsumerTraits(test_case, future.GetCallback());
    EXPECT_EQ(test_case, future.Get());
  }
}

TEST_F(MemoryConsumerTraitsTest, OutOfRange) {
  static constexpr base::MemoryConsumerTraits kTestCase(
      EstimatedMemoryUsage::kSmall, ReleaseMemoryCost::kRequiresTraversal,
      InformationRetention::kLossless, ExecutionType::kSynchronous,
      static_cast<SupportsMemoryLimit>(22));

  base::RunLoop run_loop;
  SetDisconnectHandler(run_loop.QuitClosure());
  remote()->EchoMemoryConsumerTraits(kTestCase, base::DoNothing());
  run_loop.Run();
}

}  // namespace content
