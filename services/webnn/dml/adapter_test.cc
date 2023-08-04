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
  EXPECT_NE(Adapter::Create(dxgi_adapter).get(), nullptr);
}

TEST_F(WebNNAdapterTest, GetInstance) {
  // Test creating Adapter instance upon `GetInstance()` and release it if there
  // are no references anymore.
  {
    auto adapter = Adapter::GetInstance();
    EXPECT_NE(adapter.get(), nullptr);
  }
  EXPECT_EQ(Adapter::instance_, nullptr);

  // Test two Adapters should share one instance.
  {
    auto adapter1 = Adapter::GetInstance();
    auto adapter2 = Adapter::GetInstance();
    EXPECT_EQ(adapter1.get(), adapter2.get());
  }
  EXPECT_EQ(Adapter::instance_, nullptr);
}

TEST_F(WebNNAdapterTest, CheckAdapterAccessors) {
  auto adapter = Adapter::GetInstance();
  ASSERT_NE(adapter.get(), nullptr);
  EXPECT_NE(adapter->dxgi_adapter(), nullptr);
  EXPECT_NE(adapter->d3d12_device(), nullptr);
  EXPECT_NE(adapter->dml_device(), nullptr);
  EXPECT_NE(adapter->command_queue(), nullptr);
}

}  // namespace webnn::dml
