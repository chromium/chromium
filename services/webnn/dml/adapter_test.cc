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
  SKIP_TEST_IF(!Adapter::GetGpuInstanceForTesting().has_value());
}

TEST_F(WebNNAdapterTest, GetGpuInstance) {
  // Test creating Adapter instance upon `GetGpuInstanceForTesting()` and
  // release it if there are no references anymore.
  { EXPECT_TRUE(Adapter::GetGpuInstanceForTesting().has_value()); }
  EXPECT_EQ(Adapter::gpu_instance_, nullptr);

  // Test two Adapters should share one instance.
  {
    auto adapter1_creation_result = Adapter::GetGpuInstanceForTesting();
    auto adapter2_creation_result = Adapter::GetGpuInstanceForTesting();
    ASSERT_TRUE(adapter1_creation_result.has_value());
    ASSERT_TRUE(adapter2_creation_result.has_value());
    EXPECT_EQ(adapter1_creation_result.value(),
              adapter2_creation_result.value());
  }
  EXPECT_EQ(Adapter::gpu_instance_, nullptr);
}

TEST_F(WebNNAdapterTest, GetNpuInstance) {
  // Skip if failed to get NPU instance since not all platforms support NPU.
  SKIP_TEST_IF(!Adapter::GetNpuInstanceForTesting().has_value());
  // Test creating Adapter instance upon `GetNpuInstance()` and release it if
  // there are no references anymore.
  { EXPECT_TRUE(Adapter::GetNpuInstanceForTesting().has_value()); }
  EXPECT_EQ(Adapter::npu_instance_, nullptr);

  // Test two Adapters should share one instance.
  {
    auto adapter1_creation_result = Adapter::GetNpuInstanceForTesting();
    auto adapter2_creation_result = Adapter::GetNpuInstanceForTesting();
    ASSERT_TRUE(adapter1_creation_result.has_value());
    ASSERT_TRUE(adapter2_creation_result.has_value());
    EXPECT_EQ(adapter1_creation_result.value(),
              adapter2_creation_result.value());
  }
  EXPECT_EQ(Adapter::npu_instance_, nullptr);
}

TEST_F(WebNNAdapterTest, CheckAdapterAccessors) {
  auto adapter_creation_result = Adapter::GetGpuInstanceForTesting();
  ASSERT_TRUE(adapter_creation_result.has_value());
  auto adapter = adapter_creation_result.value();
  EXPECT_NE(adapter->d3d12_device(), nullptr);
  EXPECT_NE(adapter->dml_device(), nullptr);
  EXPECT_NE(adapter->command_queue(), nullptr);
  EXPECT_EQ(adapter->init_command_queue_for_npu(), nullptr);
  EXPECT_EQ(adapter->init_task_runner_for_npu(), nullptr);
}

TEST_F(WebNNAdapterTest, CheckAdapterMinFeatureLevel) {
  // DML_FEATURE_LEVEL_2_0 is the minimum required feature level because that is
  // where DMLCreateDevice1 was introduced.
  auto adapter_creation_result = Adapter::GetGpuInstanceForTesting();
  ASSERT_TRUE(adapter_creation_result.has_value());
  EXPECT_TRUE(adapter_creation_result.value()->IsDMLFeatureLevelSupported(
      DML_FEATURE_LEVEL_2_0));
}

}  // namespace webnn::dml
