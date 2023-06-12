// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/windows/openxr_graphics_binding_d3d11.h"

#include <d3d11_4.h>
#include <wrl.h>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/openxr_platform.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/openxr/windows/openxr_platform_helper_windows.h"
#include "device/vr/windows/d3d11_texture_helper.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

// static
void OpenXrGraphicsBinding::GetRequiredExtensions(
    std::vector<const char*>& extensions) {
  extensions.push_back(XR_KHR_D3D11_ENABLE_EXTENSION_NAME);
}

OpenXrGraphicsBindingD3D11::OpenXrGraphicsBindingD3D11(
    D3D11TextureHelper* texture_helper,
    base::WeakPtr<OpenXrPlatformHelperWindows> weak_platform_helper)
    : texture_helper_(texture_helper),
      weak_platform_helper_(weak_platform_helper) {}

OpenXrGraphicsBindingD3D11::~OpenXrGraphicsBindingD3D11() = default;

bool OpenXrGraphicsBindingD3D11::Initialize(XrInstance instance,
                                            XrSystemId system) {
  if (initialized_) {
    return true;
  }

  if (!texture_helper_) {
    DVLOG(1) << __func__ << " No TextureHelper";
    return false;
  }

  if (!weak_platform_helper_) {
    DVLOG(1) << __func__ << " WeakPtr failed to resolve";
    return false;
  }

  LUID luid;
  if (!weak_platform_helper_->TryGetLuid(&luid, system)) {
    DVLOG(1) << __func__ << " Did not get a luid";
    return false;
  }

  texture_helper_->SetUseBGRA(true);
  if (!texture_helper_->SetAdapterLUID(luid) ||
      !texture_helper_->EnsureInitialized()) {
    DVLOG(1) << __func__ << " Texture helper initialization failed";
    return false;
  }

  binding_.device = texture_helper_->GetDevice().Get();
  initialized_ = true;
  return true;
}

const void* OpenXrGraphicsBindingD3D11::GetSessionCreateInfo() const {
  CHECK(initialized_);
  return &binding_;
}

int64_t OpenXrGraphicsBindingD3D11::GetSwapchainFormat(
    XrSession session) const {
  // OpenXR's swapchain format expects to describe the texture content.
  // The result of a swapchain image created from OpenXR API always contains a
  // typeless texture. On the other hand, WebGL API uses CSS color convention
  // that's sRGB. The RGBA typelss texture from OpenXR swapchain image leads to
  // a linear format render target view (reference to function
  // D3D11TextureHelper::EnsureRenderTargetView in d3d11_texture_helper.cc).
  // Therefore, the content in this openxr swapchain image is in sRGB format.
  return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
}

XrResult OpenXrGraphicsBindingD3D11::EnumerateSwapchainImages(
    const XrSwapchain& color_swapchain,
    std::vector<SwapChainInfo>& color_swapchain_images) const {
  CHECK(color_swapchain != XR_NULL_HANDLE);
  CHECK(color_swapchain_images.empty());

  uint32_t chain_length;
  RETURN_IF_XR_FAILED(
      xrEnumerateSwapchainImages(color_swapchain, 0, &chain_length, nullptr));
  std::vector<XrSwapchainImageD3D11KHR> xr_color_swapchain_images(
      chain_length, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});

  RETURN_IF_XR_FAILED(xrEnumerateSwapchainImages(
      color_swapchain, xr_color_swapchain_images.size(), &chain_length,
      reinterpret_cast<XrSwapchainImageBaseHeader*>(
          xr_color_swapchain_images.data())));

  color_swapchain_images.reserve(xr_color_swapchain_images.size());
  for (const auto& swapchain_image : xr_color_swapchain_images) {
    color_swapchain_images.emplace_back(swapchain_image.texture);
  }

  return XR_SUCCESS;
}

}  // namespace device
