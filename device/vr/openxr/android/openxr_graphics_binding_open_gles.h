// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_ANDROID_OPENXR_GRAPHICS_BINDING_OPEN_GLES_H_
#define DEVICE_VR_OPENXR_ANDROID_OPENXR_GRAPHICS_BINDING_OPEN_GLES_H_

#include "device/vr/openxr/openxr_graphics_binding.h"
#include "device/vr/openxr/openxr_platform.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrGraphicsBindingOpenGLES : public OpenXrGraphicsBinding {
 public:
  OpenXrGraphicsBindingOpenGLES();
  ~OpenXrGraphicsBindingOpenGLES() override;

  const void* GetSessionCreateInfo() const override;
  int64_t GetSwapchainFormat() const override;
  XrResult EnumerateSwapchainImages(
      const XrSwapchain& color_swapchain,
      std::vector<SwapChainInfo>& color_swapchain_images) const override;

 private:
  XrGraphicsBindingOpenGLESAndroidKHR binding_{
      XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR, nullptr};
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_ANDROID_OPENXR_GRAPHICS_BINDING_OPEN_GLES_H_
