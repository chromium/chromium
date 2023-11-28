// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <d3d11.h>
#include <wrl.h>
#include <memory>

#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_angle_util_win.h"

namespace webnn::dml {

class WebNNAdapterTest : public TestBase {};

TEST_F(WebNNAdapterTest, GetDXGIAdapterFromAngle) {
  ComPtr<ID3D11Device> d3d11_device = gl::QueryD3D11DeviceObjectFromANGLE();
  ASSERT_NE(d3d11_device.Get(), nullptr);
  ComPtr<IDXGIDevice> dxgi_device;
  d3d11_device.As(&dxgi_device);
  ComPtr<IDXGIAdapter> dxgi_adapter;
  dxgi_device->GetAdapter(&dxgi_adapter);
  EXPECT_NE(dxgi_adapter.Get(), nullptr);
}

TEST_F(WebNNAdapterTest, CreateAdapterFromAngle) {
  ComPtr<ID3D11Device> d3d11_device = gl::QueryD3D11DeviceObjectFromANGLE();
  ASSERT_NE(d3d11_device.Get(), nullptr);
  ComPtr<IDXGIDevice> dxgi_device;
  d3d11_device.As(&dxgi_device);
  ComPtr<IDXGIAdapter> dxgi_adapter;
  dxgi_device->GetAdapter(&dxgi_adapter);
  ASSERT_NE(dxgi_adapter.Get(), nullptr);
  EXPECT_TRUE(Adapter::Create(dxgi_adapter, DML_FEATURE_LEVEL_1_0).has_value());
}

TEST_F(WebNNAdapterTest, GetInstance) {
  // Test creating Adapter instance upon `GetInstance()` and release it if there
  // are no references anymore.
  { EXPECT_TRUE(Adapter::GetInstanceForTesting().has_value()); }
  EXPECT_EQ(Adapter::instance_, nullptr);

  // Test two Adapters should share one instance.
  {
    auto adapter1_creation_result = Adapter::GetInstanceForTesting();
    auto adapter2_creation_result = Adapter::GetInstanceForTesting();
    ASSERT_TRUE(adapter1_creation_result.has_value());
    ASSERT_TRUE(adapter2_creation_result.has_value());
    EXPECT_EQ(adapter1_creation_result.value(),
              adapter2_creation_result.value());
  }
  EXPECT_EQ(Adapter::instance_, nullptr);
}

TEST_F(WebNNAdapterTest, CheckAdapterAccessors) {
  auto adapter_creation_result = Adapter::GetInstanceForTesting();
  ASSERT_TRUE(adapter_creation_result.has_value());
  auto adapter = adapter_creation_result.value();
  EXPECT_NE(adapter->dxgi_adapter(), nullptr);
  EXPECT_NE(adapter->d3d12_device(), nullptr);
  EXPECT_NE(adapter->dml_device(), nullptr);
  EXPECT_NE(adapter->command_queue(), nullptr);
}

TEST_F(WebNNAdapterTest, CreateAdapterMinRequiredFeatureLevel) {
  SKIP_TEST_IF(!Adapter::GetInstance(DML_FEATURE_LEVEL_4_0).has_value());
  ASSERT_TRUE(Adapter::GetInstance(DML_FEATURE_LEVEL_4_0).has_value());
  ASSERT_TRUE(Adapter::GetInstance(DML_FEATURE_LEVEL_2_0).has_value());
  EXPECT_EQ(Adapter::GetInstance(DML_FEATURE_LEVEL_4_0).value(),
            Adapter::GetInstance(DML_FEATURE_LEVEL_2_0).value());
}

TEST_F(WebNNAdapterTest, CheckAdapterMinFeatureLevel) {
  // Check adapter feature level requested is supported.
  // All DML adapters must support DML_FEATURE_LEVEL_1_0.
  auto adapter_creation_result = Adapter::GetInstance(DML_FEATURE_LEVEL_1_0);
  ASSERT_TRUE(adapter_creation_result.has_value());
  EXPECT_TRUE(adapter_creation_result.value()->IsDMLFeatureLevelSupported(
      DML_FEATURE_LEVEL_1_0));
}

TEST_F(WebNNAdapterTest, CheckAdapterMinRequiredFeatureLevel) {
  // Check adapter feature level, if DML_FEATURE_LEVEL_4_0 is supported.
  SKIP_TEST_IF(!Adapter::GetInstance(DML_FEATURE_LEVEL_4_0).has_value());
  auto adapter_creation_result = Adapter::GetInstance(DML_FEATURE_LEVEL_4_0);
  ASSERT_TRUE(adapter_creation_result.has_value());
  EXPECT_TRUE(adapter_creation_result.value()->IsDMLFeatureLevelSupported(
      DML_FEATURE_LEVEL_4_0));
  EXPECT_TRUE(adapter_creation_result.value()->IsDMLFeatureLevelSupported(
      DML_FEATURE_LEVEL_3_0));
}

TEST_F(WebNNAdapterTest,
       CheckAdapterWithPlatformFeatureLevelLowerThanRequired) {
  // Currently, DML_FEATURE_LEVEL_5_0 is not supported.
  auto adapter_creation_result = Adapter::GetInstance(DML_FEATURE_LEVEL_5_0);
  EXPECT_FALSE(adapter_creation_result.has_value());
  EXPECT_EQ(adapter_creation_result.error()->error_code,
            mojom::Error::Code::kNotSupportedError);
  EXPECT_EQ(adapter_creation_result.error()->error_message,
            "DirectML: The DirectML feature level on this platform is lower "
            "than the minimum required one.");
}

}  // namespace webnn::dml
