// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/user_level_memory_pressure_signal_generator.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {
namespace user_level_memory_pressure_signal_generator_test {

using testing::_;

// Mock that allows setting mock memory usage.
class MockMemoryUsageMonitor : public MemoryUsageMonitor {
 public:
  MockMemoryUsageMonitor() = default;
  ~MockMemoryUsageMonitor() override = default;

  MemoryUsage GetCurrentMemoryUsage() override { return mock_memory_usage_; }

  // MemoryUsageMonitor will report the current memory usage as this value.
  void SetMockMemoryUsage(MemoryUsage usage) { mock_memory_usage_ = usage; }

 private:
  MemoryUsage mock_memory_usage_;
};

class MockUserLevelMemoryPressureSignalGenerator
    : public UserLevelMemoryPressureSignalGenerator {
 public:
  MockUserLevelMemoryPressureSignalGenerator() {
    ON_CALL(*this, Generate(_))
        .WillByDefault(testing::Invoke(
            this, &MockUserLevelMemoryPressureSignalGenerator::RealGenerate));
  }
  ~MockUserLevelMemoryPressureSignalGenerator() override = default;

  MOCK_METHOD1(Generate, void(MemoryUsage));

  void RealGenerate(MemoryUsage usage) {
    UserLevelMemoryPressureSignalGenerator::Generate(usage);
  }

  using UserLevelMemoryPressureSignalGenerator::OnRAILModeChanged;
};

class ScopedMockMemoryUsageMonitor {
 public:
  ScopedMockMemoryUsageMonitor(MemoryUsageMonitor* monitor) {
    MemoryUsageMonitor::SetInstanceForTesting(monitor);
  }
  ~ScopedMockMemoryUsageMonitor() {
    MemoryUsageMonitor::SetInstanceForTesting(nullptr);
  }
};

class UserLevelMemoryPressureSignalGeneratorTest : public testing::Test {
 public:
  UserLevelMemoryPressureSignalGeneratorTest() = default;

  void SetUp() override {
    std::map<std::string, std::string> feature_parameters;
    feature_parameters["param_512mb_device_memory_threshold_mb"] = "1024.0";
    feature_parameters["param_1gb_device_memory_threshold_mb"] = "1024.0";
    feature_parameters["param_2gb_device_memory_threshold_mb"] = "1024.0";
    feature_parameters["param_3gb_device_memory_threshold_mb"] = "1024.0";
    feature_parameters["param_4gb_device_memory_threshold_mb"] = "1024.0";
    feature_parameters["minimum_interval_s"] = "600.0";

    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kUserLevelMemoryPressureSignal, feature_parameters);

    test_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  }

  void AdvanceClock(base::TimeDelta delta) {
    test_task_runner_->FastForwardBy(delta);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
};

constexpr double kMemoryThresholdBytes = 1024 * 1024 * 1024;

TEST_F(UserLevelMemoryPressureSignalGeneratorTest, GeneratesWhenOverThreshold) {
  {
    std::unique_ptr<MockMemoryUsageMonitor> mock_memory_usage_monitor =
        std::make_unique<MockMemoryUsageMonitor>();
    ScopedMockMemoryUsageMonitor mock_memory_usage_scope(
        mock_memory_usage_monitor.get());
    MockUserLevelMemoryPressureSignalGenerator generator;
    generator.SetTickClockForTesting(test_task_runner_->GetMockTickClock());
    {
      EXPECT_CALL(generator, Generate(_)).Times(0);
      MemoryUsage usage;
      usage.v8_bytes = 0;
      usage.blink_gc_bytes = 0;
      usage.partition_alloc_bytes = 0;
      usage.private_footprint_bytes = kMemoryThresholdBytes - 1024 * 1024;
      usage.swap_bytes = 0;
      usage.vm_size_bytes = 0;
      mock_memory_usage_monitor->SetMockMemoryUsage(usage);
      AdvanceClock(base::TimeDelta::FromSeconds(1));
      test::RunDelayedTasks(base::TimeDelta::FromSeconds(1));
    }
    {
      EXPECT_CALL(generator, Generate(_)).Times(1);
      MemoryUsage usage;
      usage.v8_bytes = 0;
      usage.blink_gc_bytes = 0;
      usage.partition_alloc_bytes = 0;
      usage.private_footprint_bytes = kMemoryThresholdBytes + 1024 * 1024;
      usage.swap_bytes = 0;
      usage.vm_size_bytes = 0;
      mock_memory_usage_monitor->SetMockMemoryUsage(usage);
      AdvanceClock(base::TimeDelta::FromMinutes(10));
      test::RunDelayedTasks(base::TimeDelta::FromSeconds(1));
    }
  }
}

TEST_F(UserLevelMemoryPressureSignalGeneratorTest, GenerationPauses) {
  {
    std::unique_ptr<MockMemoryUsageMonitor> mock_memory_usage_monitor =
        std::make_unique<MockMemoryUsageMonitor>();
    ScopedMockMemoryUsageMonitor mock_memory_usage_scope(
        mock_memory_usage_monitor.get());
    MockUserLevelMemoryPressureSignalGenerator generator;
    generator.SetTickClockForTesting(test_task_runner_->GetMockTickClock());
    {
      MemoryUsage usage;
      usage.v8_bytes = 0;
      usage.blink_gc_bytes = 0;
      usage.partition_alloc_bytes = 0;
      usage.private_footprint_bytes = kMemoryThresholdBytes + 1024 * 1024;
      usage.swap_bytes = 0;
      usage.vm_size_bytes = 0;
      mock_memory_usage_monitor->SetMockMemoryUsage(usage);
      AdvanceClock(base::TimeDelta::FromMinutes(10));
      // Generated
      {
        EXPECT_CALL(generator, Generate(_)).Times(1);
        test::RunDelayedTasks(base::TimeDelta::FromSeconds(1));
      }

      AdvanceClock(base::TimeDelta::FromMinutes(1));
      // Not generated because too soon
      {
        EXPECT_CALL(generator, Generate(_)).Times(0);
        test::RunDelayedTasks(base::TimeDelta::FromSeconds(1));
      }

      AdvanceClock(base::TimeDelta::FromMinutes(10));
      generator.OnRAILModeChanged(RAILMode::kLoad);
      // Not generated because loading
      {
        EXPECT_CALL(generator, Generate(_)).Times(0);
        test::RunDelayedTasks(base::TimeDelta::FromSeconds(1));
      }

      generator.OnRAILModeChanged(RAILMode::kAnimation);
      // Generated
      {
        EXPECT_CALL(generator, Generate(_)).Times(1);
        test::RunDelayedTasks(base::TimeDelta::FromSeconds(1));
      }
    }
  }
}

}  // namespace user_level_memory_pressure_signal_generator_test
}  // namespace blink
