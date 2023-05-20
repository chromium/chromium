// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_ANDROID_OPENXR_GRAPHICS_BINDING_OPEN_GLES_H_
#define DEVICE_VR_OPENXR_ANDROID_OPENXR_GRAPHICS_BINDING_OPEN_GLES_H_

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "device/vr/openxr/openxr_graphics_binding.h"
#include "device/vr/openxr/openxr_platform.h"
#include "device/vr/vr_export.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace gl {
class GLContext;
class GLSurface;
class GLContextEGL;
}  // namespace gl

namespace device {

// OpenGLES version of the OpenXrGraphicsBinding. Used to manage rendering when
// using OpenGLES with OpenXR.
class DEVICE_VR_EXPORT OpenXrGraphicsBindingOpenGLES
    : public OpenXrGraphicsBinding {
 public:
  OpenXrGraphicsBindingOpenGLES();
  ~OpenXrGraphicsBindingOpenGLES() override;

  // OpenXrGraphicsBinding
  bool Initialize(XrInstance instance, XrSystemId system) override;
  const void* GetSessionCreateInfo() const override;
  int64_t GetSwapchainFormat(XrSession session) const override;
  XrResult EnumerateSwapchainImages(
      const XrSwapchain& color_swapchain,
      std::vector<SwapChainInfo>& color_swapchain_images) const override;

 private:
  bool initialized_ = false;
  XrGraphicsBindingOpenGLESAndroidKHR binding_{
      XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR, nullptr};

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLContextEGL> egl_context_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_ANDROID_OPENXR_GRAPHICS_BINDING_OPEN_GLES_H_
