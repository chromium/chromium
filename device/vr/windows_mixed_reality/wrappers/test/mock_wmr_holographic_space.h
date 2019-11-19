// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_HOLOGRAPHIC_SPACE_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_HOLOGRAPHIC_SPACE_H_

#include <d3d11.h>
#include "device/vr/windows_mixed_reality/wrappers/wmr_holographic_space.h"

namespace device {

class MockWMRHolographicSpace : public WMRHolographicSpace {
 public:
  MockWMRHolographicSpace();
  ~MockWMRHolographicSpace() override;

  ABI::Windows::Graphics::Holographic::HolographicAdapterId PrimaryAdapterId()
      override;
  std::unique_ptr<WMRHolographicFrame> TryCreateNextFrame() override;
  bool TrySetDirect3D11Device(
      const Microsoft::WRL::ComPtr<
          ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>& device)
      override;
  ABI::Windows::Graphics::Holographic::HolographicSpaceUserPresence
  UserPresence() override;
  std::unique_ptr<base::CallbackList<void()>::Subscription>
  AddUserPresenceChangedCallback(
      const base::RepeatingCallback<void()>& cb) override;

 private:
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_ = nullptr;
  base::CallbackList<void()> user_presence_changed_callback_list_;

  DISALLOW_COPY_AND_ASSIGN(MockWMRHolographicSpace);
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_HOLOGRAPHIC_SPACE_H_
