// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_LINUX_OPENXR_GRAPHICS_BINDING_VULKAN_H_
#define DEVICE_VR_OPENXR_LINUX_OPENXR_GRAPHICS_BINDING_VULKAN_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "device/vr/openxr/linux/openxr_vulkan_context.h"
#include "device/vr/openxr/openxr_graphics_binding.h"
#include "device/vr/openxr/openxr_platform.h"
#include "device/vr/vr_export.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {
class ClientSharedImage;
struct SharedImageInfo;
}  // namespace gpu

namespace device {

// Vulkan-backed OpenXrGraphicsBinding for Linux; uses XR_KHR_vulkan_enable2
// to drive XrSession creation with a Vulkan graphics binding.
class DEVICE_VR_EXPORT OpenXrGraphicsBindingVulkan
    : public OpenXrGraphicsBinding {
 public:
  explicit OpenXrGraphicsBindingVulkan(
      const OpenXrExtensionEnumeration* extension_enum);

  OpenXrGraphicsBindingVulkan(const OpenXrGraphicsBindingVulkan&) = delete;
  OpenXrGraphicsBindingVulkan& operator=(const OpenXrGraphicsBindingVulkan&) =
      delete;

  ~OpenXrGraphicsBindingVulkan() override;

  // OpenXrGraphicsBinding:
  bool Initialize(XrInstance instance, XrSystemId system) override;
  const void* GetSessionCreateInfo() const override;
  int64_t GetSwapchainFormat(XrSession session) const override;
  XrResult EnumerateSwapchainImages(OpenXrCompositionLayer& layer) override;
  bool CanUseSharedImages() const override;
  bool RequiresSharedImages() const override;
  void CleanupWithoutSubmit() override;
  gfx::Size GetMaxTextureSize() override;
  bool SetOverlayTexture(gfx::GpuMemoryBufferHandle texture,
                         const gpu::SyncToken& sync_token,
                         const gfx::RectF& left,
                         const gfx::RectF& right) override;
  void OnSwapchainImageReady(OpenXrCompositionLayer& layer,
                             gpu::SharedImageInterface* sii) override;
  bool SupportsLayers() const override;
  void ResizeSharedBuffer(OpenXrCompositionLayer& layer,
                          OpenXrSwapchainInfo& swap_chain_info,
                          gpu::SharedImageInterface* sii) override;

 protected:
  // OpenXrGraphicsBinding:
  bool WaitOnFence(OpenXrCompositionLayer& layer,
                   gfx::GpuFence& gpu_fence) override;
  bool RenderLayer(
      OpenXrCompositionLayer& layer,
      const scoped_refptr<viz::ContextProvider>& context_provider) override;
  void CreateSharedImages(OpenXrCompositionLayer& layer,
                          gpu::SharedImageInterface* sii) override;
  bool ShouldFlipSubmittedImage(OpenXrCompositionLayer& layer) const override;
  std::unique_ptr<OpenXrCompositionLayer::GraphicsBindingData>
  CreateLayerGraphicsBindingData() const override;

 private:
  // Preferred export path: asks the GPU process for an Ozone/GBM native pixmap
  // described by `si_info` and imports its DMA-BUF into `out_image`. Returns
  // null (and releases the allocation) if it cannot be imported.
  scoped_refptr<gpu::ClientSharedImage> ImportGbmImageAsSharedImage(
      gpu::SharedImageInterface* sii,
      const gpu::SharedImageInfo& si_info,
      const gfx::Size& size,
      VkFormat format,
      VkImageUsageFlags usage,
      OpenXrVulkanContext::IntermediateImage* out_image);

  // Exports a LINEAR intermediate image's VkDeviceMemory as a modifier-0
  // DMA-BUF wrapped in a ClientSharedImage described by `si_info`. Returns
  // null on failure; the caller owns cleanup of `image`.
  scoped_refptr<gpu::ClientSharedImage> ExportLinearImageAsSharedImage(
      gpu::SharedImageInterface* sii,
      const gpu::SharedImageInfo& si_info,
      const OpenXrVulkanContext::IntermediateImage& image);

  // Destroys all intermediate images and frees their Vulkan memory.
  void DestroyIntermediateImages();

  // Owns the Vulkan instance/device/queue plus the image create/import/copy
  // logic; created in Initialize(). All raw Vulkan calls happen inside it.
  std::unique_ptr<OpenXrVulkanContext> vulkan_;

  // Intermediate images owned by Chromium for SharedImage export.
  std::vector<OpenXrVulkanContext::IntermediateImage> intermediate_images_;

  // The format negotiated by GetSwapchainFormat(), cached so that
  // CreateSharedImages() creates matching intermediate images.
  mutable VkFormat swapchain_format_ = VK_FORMAT_UNDEFINED;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_LINUX_OPENXR_GRAPHICS_BINDING_VULKAN_H_
