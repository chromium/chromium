// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/adapter.h"

#include <memory>

#include "services/webnn/dml/test_base.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/microsoft_dxheaders/src/include/directx/dxcore.h"

// Windows SDK headers should be included after DirectX headers.
#include <d3d11.h>
#include <wrl.h>

namespace webnn::dml {

class WebNNAdapterTest : public TestBase {
 public:
  void SetUp() override;
};

void WebNNAdapterTest::SetUp() {
  SKIP_TEST_IF(!UseGPUInTests());
  Adapter::EnableDebugLayerForTesting();
  // If the adapter creation result has no value, it's most likely because
  // platform functions were not properly loaded.
  SKIP_TEST_IF(!Adapter::GetInstanceForTesting().has_value());
}

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
  EXPECT_EQ(adapter->init_command_queue_for_npu(), nullptr);
  EXPECT_EQ(adapter->init_task_runner_for_npu(), nullptr);
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
  // DML_FEATURE_LEVEL_2_0 is the minimum required feature level because that is
  // where DMLCreateDevice1 was introduced.
  auto adapter_creation_result =
      Adapter::GetInstanceForTesting(DML_FEATURE_LEVEL_2_0);
  ASSERT_TRUE(adapter_creation_result.has_value());
  EXPECT_TRUE(adapter_creation_result.value()->IsDMLFeatureLevelSupported(
      DML_FEATURE_LEVEL_2_0));
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
  EXPECT_TRUE(adapter_creation_result.value()->IsDMLFeatureLevelSupported(
      DML_FEATURE_LEVEL_2_0));
  EXPECT_TRUE(adapter_creation_result.value()->IsDMLFeatureLevelSupported(
      DML_FEATURE_LEVEL_1_0));
}

}  // namespace webnn::dml
