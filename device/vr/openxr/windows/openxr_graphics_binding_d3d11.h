// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_WINDOWS_OPENXR_GRAPHICS_BINDING_D3D11_H_
#define DEVICE_VR_OPENXR_WINDOWS_OPENXR_GRAPHICS_BINDING_D3D11_H_

#include <d3d11_4.h>
#include <wrl.h>

#include "base/memory/weak_ptr.h"
#include "device/vr/openxr/openxr_graphics_binding.h"
#include "device/vr/openxr/openxr_platform.h"
#include "device/vr/windows/d3d11_texture_helper.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrPlatformHelperWindows;

// D3D11 implementation of the OpenXrGraphicsBinding. Used to manage rendering
// when using D3D11 with OpenXR.
class OpenXrGraphicsBindingD3D11 : public OpenXrGraphicsBinding {
 public:
  explicit OpenXrGraphicsBindingD3D11(
      base::WeakPtr<OpenXrPlatformHelperWindows> weak_platform_helper);
  ~OpenXrGraphicsBindingD3D11() override;

  // OpenXrGraphicsBinding
  bool Initialize(XrInstance instance, XrSystemId system) override;
  const void* GetSessionCreateInfo() const override;
  int64_t GetSwapchainFormat(XrSession session) const override;
  XrResult EnumerateSwapchainImages(OpenXrCompositionLayer& layer) override;
  bool CanUseSharedImages() const override;
  void ResizeSharedBuffer(OpenXrCompositionLayer&,
                          OpenXrSwapchainInfo& swap_chain_info,
                          gpu::SharedImageInterface* sii) override;
  void OnSwapchainImageSizeChanged(OpenXrCompositionLayer&) override;
  void OnSwapchainImageActivated(OpenXrCompositionLayer&,
                                 gpu::SharedImageInterface* sii) override;
  void CleanupWithoutSubmit() override;
  void OnSetOverlayAndWebXrVisibility() override;
  void SetWebXrTexture(mojo::PlatformHandle texture_handle,
                       const gpu::SyncToken& sync_token,
                       const gfx::RectF& left,
                       const gfx::RectF& right) override;
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
  void CreateSharedImages(OpenXrCompositionLayer& layer,
                          gpu::SharedImageInterface* sii) override;
  bool WaitOnFence(OpenXrCompositionLayer& layer,
                   gfx::GpuFence& gpu_fence) override;
  bool ShouldFlipSubmittedImage(OpenXrCompositionLayer& layer) const override;
  bool SupportsLayers() const override;
  std::unique_ptr<OpenXrCompositionLayer::GraphicsBindingData>
  CreateLayerGraphicsBindingData() const override;

  bool initialized_ = false;
  std::unique_ptr<D3D11TextureHelper> texture_helper_;
  base::WeakPtr<OpenXrPlatformHelperWindows> weak_platform_helper_;

  XrGraphicsBindingD3D11KHR binding_{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR,
                                     nullptr, nullptr};
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_WINDOWS_OPENXR_GRAPHICS_BINDING_D3D11_H_
