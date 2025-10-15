// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_ANDROID_OPENXR_GRAPHICS_BINDING_OPEN_GLES_H_
#define DEVICE_VR_OPENXR_ANDROID_OPENXR_GRAPHICS_BINDING_OPEN_GLES_H_

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "device/vr/android/local_texture.h"
#include "device/vr/android/xr_renderer.h"
#include "device/vr/openxr/openxr_graphics_binding.h"
#include "device/vr/openxr/openxr_platform.h"
#include "device/vr/vr_export.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "ui/gl/gl_bindings.h"

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
  explicit OpenXrGraphicsBindingOpenGLES(
      const OpenXrExtensionEnumeration* extension_enum);
  ~OpenXrGraphicsBindingOpenGLES() override;

  // Initialize the GL configurations, making this ready to use.
  bool InitializeGl();

  // OpenXrGraphicsBinding
  bool Initialize(XrInstance instance, XrSystemId system) override;
  const void* GetSessionCreateInfo() const override;
  int64_t GetSwapchainFormat(XrSession session) const override;
  XrResult EnumerateSwapchainImages(OpenXrCompositionLayer& layer) override;
  bool CanUseSharedImages() const override;
  void OnSwapchainImageActivated(OpenXrCompositionLayer& layer,
                                 gpu::SharedImageInterface* sii) override;
  void ResizeSharedBuffer(OpenXrCompositionLayer& layer,
                          OpenXrSwapchainInfo& swap_chain_info,
                          gpu::SharedImageInterface* sii) override;
  void CleanupWithoutSubmit() override;
  bool SetOverlayTexture(gfx::GpuMemoryBufferHandle texture,
                         const gpu::SyncToken& sync_token,
                         const gfx::RectF& left,
                         const gfx::RectF& right) override;
  gfx::Size GetMaxTextureSize() override;

 protected:
  // OpenXrGraphicsBinding
  bool RenderLayer(
      OpenXrCompositionLayer& layer,
      const scoped_refptr<viz::ContextProvider>& context_provider) override;
  bool WaitOnFence(OpenXrCompositionLayer& layer,
                   gfx::GpuFence& gpu_fence) override;
  void CreateSharedImages(OpenXrCompositionLayer& layer,
                          gpu::SharedImageInterface* sii) override;
  bool ShouldFlipSubmittedImage(OpenXrCompositionLayer& layer) const override;
  bool SupportsLayers() const override;
  std::unique_ptr<OpenXrCompositionLayer::GraphicsBindingData>
  CreateLayerGraphicsBindingData() const override;

  bool gl_initialized_ = false;
  bool initialized_ = false;
  XrGraphicsBindingOpenGLESAndroidKHR binding_{
      XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR, nullptr};
  gfx::GpuMemoryBufferHandle overlay_handle_;

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLContextEGL> egl_context_;

  std::unique_ptr<XrRenderer> renderer_;
  LocalTexture overlay_texture_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_ANDROID_OPENXR_GRAPHICS_BINDING_OPEN_GLES_H_
