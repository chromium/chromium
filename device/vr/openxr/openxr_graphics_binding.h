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

// This class exists to provide an abstraction for the different rendering
// paths that can be taken by OpenXR (e.g. DirectX vs. GLES). Any OpenXr methods
// that need types specific for a given renderer type should go through this
// interface.
class OpenXrGraphicsBinding {
 public:
  // Gets the set of RequiredExtensions that need to be present on the platform.
  static void GetRequiredExtensions(std::vector<const char*>& extensions);

  virtual ~OpenXrGraphicsBinding() = default;

  // Ensures that the GraphicsBinding is ready for use.
  virtual bool Initialize(XrInstance instance, XrSystemId system) = 0;

  // Gets a pointer to a platform-specific XrGraphicsBindingFoo. The pointer is
  // guaranteed to live as long as this class does.
  virtual const void* GetSessionCreateInfo() const = 0;

  // Gets the format that we expect from the platform swapchain.
  virtual int64_t GetSwapchainFormat(XrSession session) const = 0;

  // Calls xrEnumerateSwapChain and stores all relevant data in the passed in
  // array of `SwapChainInfo`s.
  virtual XrResult EnumerateSwapchainImages(
      const XrSwapchain& color_swapchain,
      std::vector<SwapChainInfo>& color_swapchain_images) const = 0;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_GRAPHICS_BINDING_H_
