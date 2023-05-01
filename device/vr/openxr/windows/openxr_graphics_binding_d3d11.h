// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_WINDOWS_OPENXR_GRAPHICS_BINDING_D3D11_H_
#define DEVICE_VR_OPENXR_WINDOWS_OPENXR_GRAPHICS_BINDING_D3D11_H_

#include <d3d11_4.h>
#include <wrl.h>

#include "device/vr/openxr/openxr_graphics_binding.h"
#include "device/vr/openxr/openxr_platform.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrGraphicsBindingD3D11 : public OpenXrGraphicsBinding {
 public:
  explicit OpenXrGraphicsBindingD3D11(
      const Microsoft::WRL::ComPtr<ID3D11Device>& device);
  ~OpenXrGraphicsBindingD3D11() override;

  const void* GetSessionCreateInfo() const override;
  int64_t GetSwapchainFormat() const override;
  XrResult EnumerateSwapchainImages(
      const XrSwapchain& color_swapchain,
      std::vector<SwapChainInfo>& color_swapchain_images) const override;

 private:
  Microsoft::WRL::ComPtr<ID3D11Device> device_;

  XrGraphicsBindingD3D11KHR binding_{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR,
                                     nullptr, nullptr};
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_WINDOWS_OPENXR_GRAPHICS_BINDING_D3D11_H_
