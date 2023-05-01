// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_GRAPHICS_BINDING_H_
#define DEVICE_VR_OPENXR_OPENXR_GRAPHICS_BINDING_H_

#include <cstdint>
#include <vector>

#include "third_party/openxr/src/include/openxr/openxr.h"
namespace device {

struct SwapChainInfo;

class OpenXrGraphicsBinding {
 public:
  virtual ~OpenXrGraphicsBinding() = default;

  // Gets a pointer to a platform-specific XrGraphicsBindingFoo. The pointer is
  // guaranteed to live as long as this class does.
  virtual const void* GetSessionCreateInfo() const = 0;
  virtual int64_t GetSwapchainFormat() const = 0;
  virtual XrResult EnumerateSwapchainImages(
      const XrSwapchain& color_swapchain,
      std::vector<SwapChainInfo>& color_swapchain_images) const = 0;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_GRAPHICS_BINDING_H_
