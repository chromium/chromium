// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/vr/openxr/android/openxr_graphics_binding_open_gles.h"

#include <vector>

#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/openxr_platform.h"
#include "device/vr/openxr/openxr_util.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "ui/gfx/buffer_types.h"

namespace device {

// static
void OpenXrGraphicsBinding::GetRequiredExtensions(
    std::vector<const char*>& extensions) {
  extensions.push_back(XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME);
}

OpenXrGraphicsBindingOpenGLES::OpenXrGraphicsBindingOpenGLES() = default;
OpenXrGraphicsBindingOpenGLES::~OpenXrGraphicsBindingOpenGLES() = default;

bool OpenXrGraphicsBindingOpenGLES::Initialize() {
  return true;
}

const void* OpenXrGraphicsBindingOpenGLES::GetSessionCreateInfo() const {
  return &binding_;
}

int64_t OpenXrGraphicsBindingOpenGLES::GetSwapchainFormat() const {
  return GL_RGBA;
}

XrResult OpenXrGraphicsBindingOpenGLES::EnumerateSwapchainImages(
    const XrSwapchain& color_swapchain,
    std::vector<SwapChainInfo>& color_swapchain_images) const {
  CHECK(color_swapchain == XR_NULL_HANDLE);
  CHECK(color_swapchain_images.empty());

  uint32_t chain_length;
  RETURN_IF_XR_FAILED(
      xrEnumerateSwapchainImages(color_swapchain, 0, &chain_length, nullptr));
  std::vector<XrSwapchainImageOpenGLESKHR> xr_color_swapchain_images(
      chain_length, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});

  RETURN_IF_XR_FAILED(xrEnumerateSwapchainImages(
      color_swapchain, xr_color_swapchain_images.size(), &chain_length,
      reinterpret_cast<XrSwapchainImageBaseHeader*>(
          xr_color_swapchain_images.data())));

  color_swapchain_images.reserve(xr_color_swapchain_images.size());
  for (size_t i = 0; i < xr_color_swapchain_images.size(); i++) {
    color_swapchain_images.emplace_back();
  }

  return XR_SUCCESS;
}

}  // namespace device
