// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/vulkan_in_process_context_provider.h"

#include "base/functional/callback_helpers.h"
#include "base/memory_coordinator/memory_coordinator_features.h"
#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "base/memory_coordinator/utils.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class VulkanInProcessContextProviderTest : public testing::Test {
 public:
  VulkanInProcessContextProviderTest() {
    feature_list_.InitAndEnableFeature(base::kStatefulMemoryPressure);
  }

  void CreateVulkanInProcessContextProvider(
      uint32_t sync_cpu_memory_limit,
      base::TimeDelta cooldown_duration_at_memory_pressure_critical =
          base::Seconds(15)) {
    context_provider_ = new VulkanInProcessContextProvider(
        nullptr, 0, sync_cpu_memory_limit,
        cooldown_duration_at_memory_pressure_critical);
  }

  void TearDown() override { context_provider_.reset(); }

  void SendMemoryPressureSignal(base::MemoryLimit memory_limit) {
    base::RunLoop run_loop;
    registry_.NotifyUpdateMemoryLimitAsync(
        memory_limit,
        base::BindOnce(
            &base::TestMemoryConsumerRegistry::NotifyReleaseMemoryAsync,
            base::Unretained(&registry_), run_loop.QuitClosure()));
    run_loop.Run();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  base::TestMemoryConsumerRegistry registry_;
  scoped_refptr<VulkanInProcessContextProvider> context_provider_;
};

TEST_F(VulkanInProcessContextProviderTest,
       NotifyMemoryPressureChangesSyncCpuMemoryLimit) {
  const uint32_t kTestSyncCpuMemoryLimit = 1200;
  CreateVulkanInProcessContextProvider(kTestSyncCpuMemoryLimit);

  // Initial state.
  auto limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_THAT(limit, testing::Optional(kTestSyncCpuMemoryLimit));

  // Critical pressure -> 0% limit.
  SendMemoryPressureSignal(base::MemoryLimit::CriticalPressureThreshold());
  limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_THAT(limit, testing::Optional(0u));

  // Pressure subsides -> 100% limit.
  SendMemoryPressureSignal(base::MemoryLimit::NoPressureThreshold());
  limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_THAT(limit, testing::Optional(kTestSyncCpuMemoryLimit));

  // Moderate pressure -> 50% limit.
  SendMemoryPressureSignal(base::MemoryLimit::ModeratePressureThreshold());
  limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_THAT(limit, testing::Optional(kTestSyncCpuMemoryLimit / 2));
}

TEST_F(VulkanInProcessContextProviderTest,
       ZeroSyncCpuMemoryLimitDoesNotChange) {
  CreateVulkanInProcessContextProvider(0);

  auto limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_FALSE(limit.has_value());

  SendMemoryPressureSignal(base::MemoryLimit::CriticalPressureThreshold());
  limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_FALSE(limit.has_value());

  SendMemoryPressureSignal(base::MemoryLimit::ModeratePressureThreshold());
  limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_FALSE(limit.has_value());
}

TEST_F(VulkanInProcessContextProviderTest,
       NotifyMemoryPressureStatelessCooldown) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(base::kStatefulMemoryPressure);

  const uint32_t kTestSyncCpuMemoryLimit = 1234;
  CreateVulkanInProcessContextProvider(kTestSyncCpuMemoryLimit,
                                       base::Seconds(15));

  // Initial state.
  auto limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_THAT(limit, testing::Optional(kTestSyncCpuMemoryLimit));

  // Critical pressure -> 0 limit.
  SendMemoryPressureSignal(base::MemoryLimit::CriticalPressureThreshold());
  limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_THAT(limit, testing::Optional(0u));

  // Pressure level subsides, but we are still in the cooldown period.
  SendMemoryPressureSignal(base::MemoryLimit::NoPressureThreshold());
  limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_THAT(limit, testing::Optional(0u));

  // Reset the provider with zero cooldown to verify restoration.
  CreateVulkanInProcessContextProvider(kTestSyncCpuMemoryLimit,
                                       base::TimeDelta());
  SendMemoryPressureSignal(base::MemoryLimit::CriticalPressureThreshold());
  limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_THAT(limit, testing::Optional(kTestSyncCpuMemoryLimit));
}

TEST_F(VulkanInProcessContextProviderTest,
       StatefulMemoryPressureLimitApplicationWithZeroUsage) {
  const uint32_t kTestSyncCpuMemoryLimit = 1200;
  CreateVulkanInProcessContextProvider(kTestSyncCpuMemoryLimit);

  // Initial state.
  auto limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_THAT(limit, testing::Optional(kTestSyncCpuMemoryLimit));

  // 1. Decrease limit to 50%.
  // Since usage is 0 (< 50% target), the limit is applied immediately:
  // std::max(0, 600) = 600.
  {
    base::RunLoop run_loop;
    registry_.NotifyUpdateMemoryLimitAsync(
        base::MemoryLimit::ModeratePressureThreshold(), run_loop.QuitClosure());
    run_loop.Run();
  }
  limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_THAT(limit, testing::Optional(kTestSyncCpuMemoryLimit / 2));

  // 2. Trigger release.
  // No-op because the limit was already applied (usage < target).
  {
    base::RunLoop run_loop;
    registry_.NotifyReleaseMemoryAsync(run_loop.QuitClosure());
    run_loop.Run();
  }
  limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_THAT(limit, testing::Optional(kTestSyncCpuMemoryLimit / 2));

  // 3. Increase limit to 100%.
  // Increases are always applied immediately.
  {
    base::RunLoop run_loop;
    registry_.NotifyUpdateMemoryLimitAsync(
        base::MemoryLimit::NoPressureThreshold(), run_loop.QuitClosure());
    run_loop.Run();
  }
  limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_THAT(limit, testing::Optional(kTestSyncCpuMemoryLimit));
}

}  // namespace gpu
