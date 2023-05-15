// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_ANDROID_OPENXR_GRAPHICS_BINDING_OPEN_GLES_H_
#define DEVICE_VR_OPENXR_ANDROID_OPENXR_GRAPHICS_BINDING_OPEN_GLES_H_

#include <vector>

#include "device/vr/openxr/openxr_graphics_binding.h"
#include "device/vr/openxr/openxr_platform.h"
#include "device/vr/vr_export.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

// OpenGLES version of the OpenXrGraphicsBinding. Used to manage rendering when
// using OpenGLES with OpenXR.
class DEVICE_VR_EXPORT OpenXrGraphicsBindingOpenGLES
    : public OpenXrGraphicsBinding {
 public:
  OpenXrGraphicsBindingOpenGLES();
  ~OpenXrGraphicsBindingOpenGLES() override;

  // OpenXrGraphicsBinding
  bool Initialize() override;
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
