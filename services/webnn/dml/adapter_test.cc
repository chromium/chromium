// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <d3d11.h>
#include <dxcore.h>
#include <wrl.h>
#include <memory>

#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/test_base.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webnn::dml {

class WebNNAdapterTest : public TestBase {};

TEST_F(WebNNAdapterTest, GetGpuInstance) {
  // Test creating Adapter instance upon `GetGpuInstance()` and release it if
  // there are no references anymore.
  { EXPECT_TRUE(Adapter::GetInstanceForTesting().has_value()); }
  EXPECT_EQ(Adapter::gpu_instance_, nullptr);

  // Test two Adapters should share one instance.
  {
    auto adapter1_creation_result = Adapter::GetInstanceForTesting();
    auto adapter2_creation_result = Adapter::GetInstanceForTesting();
    ASSERT_TRUE(adapter1_creation_result.has_value());
    ASSERT_TRUE(adapter2_creation_result.has_value());
    EXPECT_EQ(adapter1_creation_result.value(),
              adapter2_creation_result.value());
  }
  EXPECT_EQ(Adapter::gpu_instance_, nullptr);
}

TEST_F(WebNNAdapterTest, GetNpuInstance) {
  // Skip if failed to get NPU instance since not all platforms support NPU.
  SKIP_TEST_IF(!Adapter::GetNpuInstance(DML_FEATURE_LEVEL_4_0).has_value());
  // Test creating Adapter instance upon `GetNpuInstance()` and release it if
  // there are no references anymore.
  { EXPECT_TRUE(Adapter::GetNpuInstance(DML_FEATURE_LEVEL_4_0).has_value()); }
  EXPECT_EQ(Adapter::npu_instance_, nullptr);

  // Test two Adapters should share one instance.
  {
    auto adapter1_creation_result =
        Adapter::GetNpuInstance(DML_FEATURE_LEVEL_4_0);
    auto adapter2_creation_result =
        Adapter::GetNpuInstance(DML_FEATURE_LEVEL_4_0);
    ASSERT_TRUE(adapter1_creation_result.has_value());
    ASSERT_TRUE(adapter2_creation_result.has_value());
    EXPECT_EQ(adapter1_creation_result.value(),
              adapter2_creation_result.value());
  }
  EXPECT_EQ(Adapter::npu_instance_, nullptr);
}

TEST_F(WebNNAdapterTest, CheckAdapterAccessors) {
  auto adapter_creation_result = Adapter::GetInstanceForTesting();
  ASSERT_TRUE(adapter_creation_result.has_value());
  auto adapter = adapter_creation_result.value();
  EXPECT_NE(adapter->d3d12_device(), nullptr);
  EXPECT_NE(adapter->dml_device(), nullptr);
  EXPECT_NE(adapter->command_queue(), nullptr);
}

TEST_F(WebNNAdapterTest, CreateAdapterMinRequiredFeatureLevel) {
  SKIP_TEST_IF(
      !Adapter::GetInstanceForTesting(DML_FEATURE_LEVEL_4_0).has_value());
  ASSERT_TRUE(
      Adapter::GetInstanceForTesting(DML_FEATURE_LEVEL_4_0).has_value());
  ASSERT_TRUE(
      Adapter::GetInstanceForTesting(DML_FEATURE_LEVEL_2_0).has_value());
  EXPECT_EQ(Adapter::GetInstanceForTesting(DML_FEATURE_LEVEL_4_0).value(),
            Adapter::GetInstanceForTesting(DML_FEATURE_LEVEL_2_0).value());
}

TEST_F(WebNNAdapterTest, CheckAdapterMinFeatureLevel) {
  // Check adapter feature level requested is supported.
  // All DML adapters must support DML_FEATURE_LEVEL_1_0.
  auto adapter_creation_result =
      Adapter::GetInstanceForTesting(DML_FEATURE_LEVEL_1_0);
  ASSERT_TRUE(adapter_creation_result.has_value());
  EXPECT_TRUE(adapter_creation_result.value()->IsDMLFeatureLevelSupported(
      DML_FEATURE_LEVEL_1_0));
}

TEST_F(WebNNAdapterTest, CheckAdapterMinRequiredFeatureLevel) {
  // Check adapter feature level, if DML_FEATURE_LEVEL_4_0 is supported.
  SKIP_TEST_IF(
      !Adapter::GetInstanceForTesting(DML_FEATURE_LEVEL_4_0).has_value());
  auto adapter_creation_result =
      Adapter::GetInstanceForTesting(DML_FEATURE_LEVEL_4_0);
  ASSERT_TRUE(adapter_creation_result.has_value());
  EXPECT_TRUE(adapter_creation_result.value()->IsDMLFeatureLevelSupported(
      DML_FEATURE_LEVEL_4_0));
  EXPECT_TRUE(adapter_creation_result.value()->IsDMLFeatureLevelSupported(
      DML_FEATURE_LEVEL_3_0));
}

TEST_F(WebNNAdapterTest,
       CheckAdapterWithPlatformFeatureLevelLowerThanRequired) {
  // Currently, DML_FEATURE_LEVEL_5_0 is not supported.
  auto adapter_creation_result =
      Adapter::GetInstanceForTesting(DML_FEATURE_LEVEL_5_0);
  EXPECT_FALSE(adapter_creation_result.has_value());
  EXPECT_EQ(adapter_creation_result.error()->code,
            mojom::Error::Code::kNotSupportedError);
  EXPECT_EQ(adapter_creation_result.error()->message,
            "DirectML: Unable to find a capable adapter.");
}

}  // namespace webnn::dml
