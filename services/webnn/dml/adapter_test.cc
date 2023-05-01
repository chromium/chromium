// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include <wrl.h>
#include <memory>

#include "base/command_line.h"
#include "services/webnn/dml/adapter.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/init/gl_factory.h"

namespace webnn::dml {

using Microsoft::WRL::ComPtr;

class WebNNAdapterTest : public testing::Test {
 public:
  void SetUp() override {
    display_ = gl::init::InitializeGLNoExtensionsOneOff(
        /*init_bindings=*/true,
        /*gpu_preference=*/gl::GpuPreference::kDefault);
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kUseGpuInTests)) {
      GTEST_SKIP() << "Skipping all tests for this fixture if GPU hardware "
                      "hasn't been used in tests.";
    }
  }

  void TearDown() override {
    gl::init::ShutdownGL(display_, /*due_to_fallback=*/false);
  }

 protected:
  raw_ptr<gl::GLDisplay> display_ = nullptr;
};

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

TEST_F(WebNNAdapterTest, CheckAdapterAccessors) {
  ComPtr<ID3D11Device> d3d11_device = gl::QueryD3D11DeviceObjectFromANGLE();
  ASSERT_NE(d3d11_device.Get(), nullptr);
  ComPtr<IDXGIDevice> dxgi_device;
  d3d11_device.As(&dxgi_device);
  ComPtr<IDXGIAdapter> dxgi_adapter;
  dxgi_device->GetAdapter(&dxgi_adapter);
  ASSERT_NE(dxgi_adapter.Get(), nullptr);
  auto adapter = Adapter::Create(dxgi_adapter);
  ASSERT_NE(adapter.get(), nullptr);
  EXPECT_NE(adapter->dxgi_adapter(), nullptr);
  EXPECT_NE(adapter->d3d12_device(), nullptr);
  EXPECT_NE(adapter->dml_device(), nullptr);
  EXPECT_NE(adapter->command_queue(), nullptr);
}

}  // namespace webnn::dml
