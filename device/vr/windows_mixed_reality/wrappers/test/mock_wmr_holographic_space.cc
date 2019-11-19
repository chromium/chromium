// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_holographic_space.h"

#include <D3D11_1.h>
#include <Windows.Graphics.DirectX.Direct3D11.interop.h>
#include <dxgi.h>
#include <wrl.h>

#include "base/stl_util.h"

#include "device/vr/windows/d3d11_device_helpers.h"
#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_holographic_frame.h"

namespace device {

MockWMRHolographicSpace::MockWMRHolographicSpace() {}

MockWMRHolographicSpace::~MockWMRHolographicSpace() = default;

ABI::Windows::Graphics::Holographic::HolographicAdapterId
MockWMRHolographicSpace::PrimaryAdapterId() {
  ABI::Windows::Graphics::Holographic::HolographicAdapterId ret;

  int32_t adapter_index = -1;
  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
  vr::GetD3D11_1Adapter(&adapter_index, &adapter);

  if (adapter_index == -1)
    return ret;

  DXGI_ADAPTER_DESC description;
  if (!SUCCEEDED(adapter->GetDesc(&description))) {
    return ret;
  }

  ret.LowPart = description.AdapterLuid.LowPart;
  ret.HighPart = description.AdapterLuid.HighPart;

  return ret;
}

std::unique_ptr<WMRHolographicFrame>
MockWMRHolographicSpace::TryCreateNextFrame() {
  return std::make_unique<MockWMRHolographicFrame>(d3d11_device_);
}

bool MockWMRHolographicSpace::TrySetDirect3D11Device(
    const Microsoft::WRL::ComPtr<
        ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>& device) {
  Microsoft::WRL::ComPtr<
      Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>
      dxgi_interface_access;
  if (FAILED(device.As(&dxgi_interface_access)))
    return false;
  if (FAILED(dxgi_interface_access->GetInterface(IID_PPV_ARGS(&d3d11_device_))))
    return false;
  return true;
}

ABI::Windows::Graphics::Holographic::HolographicSpaceUserPresence
MockWMRHolographicSpace::UserPresence() {
  return ABI::Windows::Graphics::Holographic::
      HolographicSpaceUserPresence_PresentActive;
}
std::unique_ptr<base::CallbackList<void()>::Subscription>
MockWMRHolographicSpace::AddUserPresenceChangedCallback(
    const base::RepeatingCallback<void()>& cb) {
  return user_presence_changed_callback_list_.Add(cb);
}

}  // namespace device
