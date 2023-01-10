// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <d3d11.h>
#include <mfapi.h>

#include "media/base/test_helpers.h"
#include "media/base/win/dxgi_device_manager.h"
#include "media/base/win/mf_initializer.h"

namespace media {

using Microsoft::WRL::ComPtr;

class DXGIDeviceScopedHandleTest : public testing::Test {
 public:
  DXGIDeviceScopedHandleTest() = default;
  ~DXGIDeviceScopedHandleTest() override = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(InitializeMediaFoundation());

    // Get a shared DXGI Device Manager from Media Foundation.
    ASSERT_HRESULT_SUCCEEDED(
        MFLockDXGIDeviceManager(&device_reset_token_, &dxgi_device_man_));

    // |dxgi_device_man_| does not create the device, creates Direct3D device.
    ComPtr<ID3D11Device> d3d11_device;
    UINT creation_flags =
        (D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT |
         D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS);
    static const D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1};
    ASSERT_HRESULT_SUCCEEDED(
        D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, creation_flags,
                          feature_levels, ARRAYSIZE(feature_levels),
                          D3D11_SDK_VERSION, &d3d11_device, nullptr, nullptr));

    ComPtr<ID3D10Multithread> multithreaded_device;
    ASSERT_HRESULT_SUCCEEDED(d3d11_device.As(&multithreaded_device));
    multithreaded_device->SetMultithreadProtected(TRUE);

    // Set Direct3D device to the device manager.
    ASSERT_HRESULT_SUCCEEDED(
        dxgi_device_man_->ResetDevice(d3d11_device.Get(), device_reset_token_));
  }

  void TearDown() override {
    ASSERT_HRESULT_SUCCEEDED(MFUnlockDXGIDeviceManager());
  }

  Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> dxgi_device_man_ = nullptr;
  UINT device_reset_token_ = 0;
};

TEST_F(DXGIDeviceScopedHandleTest, LockDevice) {
  {
    // Create DXGIDeviceScopedHandle in an inner scope without LockDevice
    // call.
    DXGIDeviceScopedHandle device_handle_1(dxgi_device_man_.Get());
  }
  {
    // Create DXGIDeviceScopedHandle in an inner scope with LockDevice call.
    DXGIDeviceScopedHandle device_handle_2(dxgi_device_man_.Get());
    ComPtr<ID3D11Device> device2;
    ASSERT_HRESULT_SUCCEEDED(
        device_handle_2.LockDevice(IID_PPV_ARGS(&device2)));
  }
  // Use the device in an outer scope.
  DXGIDeviceScopedHandle device_handle_3(dxgi_device_man_.Get());
  ComPtr<ID3D11Device> device3;
  ASSERT_HRESULT_SUCCEEDED(device_handle_3.LockDevice(IID_PPV_ARGS(&device3)));
}

TEST_F(DXGIDeviceScopedHandleTest, GetDevice) {
  {
    // Create DXGIDeviceScopedHandle in an inner scope.
    DXGIDeviceScopedHandle device_handle_1(dxgi_device_man_.Get());
  }
  {
    // Create DXGIDeviceScopedHandle in an inner scope with GetDevice call.
    DXGIDeviceScopedHandle device_handle_2(dxgi_device_man_.Get());
    ComPtr<ID3D11Device> device2 = device_handle_2.GetDevice();
    EXPECT_NE(device2, nullptr);
  }
  // Use the device in an outer scope.
  DXGIDeviceScopedHandle device_handle_3(dxgi_device_man_.Get());
  ComPtr<ID3D11Device> device3 = device_handle_3.GetDevice();
  EXPECT_NE(device3, nullptr);
}

}  // namespace media
