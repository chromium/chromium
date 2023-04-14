// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/cpu_probe_mac.h"

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "services/device/compute_pressure/cpu_probe.h"
#include "services/device/compute_pressure/pressure_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class CpuProbeMacTest : public testing::Test {
 public:
  CpuProbeMacTest() = default;

  ~CpuProbeMacTest() override = default;

  void SetUp() override {
    probe_ = std::make_unique<FakePlatformCpuProbe<CpuProbeMac>>(
        base::Milliseconds(10), base::DoNothing());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakePlatformCpuProbe<CpuProbeMac>> probe_;
};

TEST_F(CpuProbeMacTest, ProductionDataNoCrash) {
  probe_->Update();
  EXPECT_EQ(probe_->WaitForSample().cpu_utilization,
            CpuProbe::kUnsupportedValue.cpu_utilization)
      << "No baseline on first Update()";

  probe_->Update();
  PressureSample sample = probe_->WaitForSample();
  EXPECT_GE(sample.cpu_utilization, 0.0);
  EXPECT_LE(sample.cpu_utilization, 1.0);
}

}  // namespace device
