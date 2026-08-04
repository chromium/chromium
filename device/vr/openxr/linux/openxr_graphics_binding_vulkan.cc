// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/linux/openxr_graphics_binding_vulkan.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "device/vr/openxr/openxr_composition_layer.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_swapchain_info.h"
#include "device/vr/openxr/openxr_util.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/common/surface_handle.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace device {

namespace {

// The swapchain formats CreateSharedImages() can describe as a
// viz::SharedImageFormat, most preferred first. A runtime offering none of
// them is unusable: any other format would be misinterpreted downstream.
constexpr VkFormat kSupportedFormats[] = {
    VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB,
    VK_FORMAT_B8G8R8A8_UNORM};

}  // namespace

// static
void OpenXrGraphicsBinding::GetRequiredExtensions(
    std::vector<const char*>& extensions) {
  extensions.push_back(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);
}

OpenXrGraphicsBindingVulkan::OpenXrGraphicsBindingVulkan(
    const OpenXrExtensionEnumeration* extension_enum)
    : OpenXrGraphicsBinding(extension_enum) {}

OpenXrGraphicsBindingVulkan::~OpenXrGraphicsBindingVulkan() {
  // Free the intermediate images (which reference the VkDevice) before the
  // OpenXrVulkanContext that owns that device is destroyed with vulkan_.
  DestroyIntermediateImages();
}

bool OpenXrGraphicsBindingVulkan::Initialize(XrInstance instance,
                                             XrSystemId system) {
  if (vulkan_ && vulkan_->initialized()) {
    return true;
  }
  auto vulkan = std::make_unique<OpenXrVulkanContext>();
  if (!vulkan->Initialize(instance, system)) {
    return false;
  }
  vulkan_ = std::move(vulkan);
  return true;
}

const void* OpenXrGraphicsBindingVulkan::GetSessionCreateInfo() const {
  return (vulkan_ && vulkan_->initialized()) ? vulkan_->binding() : nullptr;
}

int64_t OpenXrGraphicsBindingVulkan::GetSwapchainFormat(
    XrSession session) const {
  uint32_t count = 0;
  if (XR_FAILED(xrEnumerateSwapchainFormats(session, 0, &count, nullptr)) ||
      count == 0) {
    return 0;
  }
  std::vector<int64_t> formats(count);
  if (XR_FAILED(xrEnumerateSwapchainFormats(session, count, &count,
                                            formats.data()))) {
    return 0;
  }

  auto supported =
      std::ranges::find_if(kSupportedFormats, [&formats](VkFormat candidate) {
        return std::ranges::contains(formats, static_cast<int64_t>(candidate));
      });
  if (supported == std::ranges::end(kSupportedFormats)) {
    DLOG(ERROR) << __func__ << ": runtime offers no usable swapchain format";
    return 0;
  }

  // Cache the negotiated format so CreateSharedImages() can create matching
  // intermediate images without needing to re-query the session.
  swapchain_format_ = *supported;
  DVLOG(1) << "OpenXR Vulkan swapchain format negotiated: "
           << swapchain_format_;
  return static_cast<int64_t>(swapchain_format_);
}

XrResult OpenXrGraphicsBindingVulkan::EnumerateSwapchainImages(
    OpenXrCompositionLayer& layer) {
  CHECK(layer.HasColorSwapchain());
  CHECK(layer.GetSwapchainImages().empty());

  uint32_t chain_length = 0;
  RETURN_IF_XR_FAILED(xrEnumerateSwapchainImages(layer.color_swapchain(), 0,
                                                 &chain_length, nullptr));

  std::vector<XrSwapchainImageVulkan2KHR> xr_color_swapchain_images(
      chain_length, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});

  RETURN_IF_XR_FAILED(xrEnumerateSwapchainImages(
      layer.color_swapchain(), xr_color_swapchain_images.size(), &chain_length,
      reinterpret_cast<XrSwapchainImageBaseHeader*>(
          xr_color_swapchain_images.data())));

  std::vector<OpenXrSwapchainInfo> color_swapchain_images;
  color_swapchain_images.reserve(xr_color_swapchain_images.size());
  for (const auto& swapchain_image : xr_color_swapchain_images) {
    color_swapchain_images.emplace_back(swapchain_image.image);
  }
  layer.SetSwapchainImages(std::move(color_swapchain_images));

  return XR_SUCCESS;
}

void OpenXrGraphicsBindingVulkan::DestroyIntermediateImages() {
  if (vulkan_ && vulkan_->valid()) {
    for (auto& img : intermediate_images_) {
      vulkan_->DestroyIntermediateImage(img);
    }
  }
  intermediate_images_.clear();
}

scoped_refptr<gpu::ClientSharedImage>
OpenXrGraphicsBindingVulkan::ImportGbmImageAsSharedImage(
    gpu::SharedImageInterface* sii,
    const gpu::SharedImageInfo& si_info,
    const gfx::Size& size,
    VkFormat format,
    VkImageUsageFlags usage,
    OpenXrVulkanContext::IntermediateImage* out_image) {
  scoped_refptr<gpu::ClientSharedImage> client_si = sii->CreateSharedImage(
      si_info, gpu::kNullSurfaceHandle, gfx::BufferUsage::SCANOUT);
  if (!client_si) {
    return nullptr;
  }
  gfx::GpuMemoryBufferHandle handle = client_si->CloneGpuMemoryBufferHandle();
  if (handle.type == gfx::NATIVE_PIXMAP &&
      vulkan_->ImportDmaBufImage(handle.native_pixmap_handle(), size, format,
                                 usage, out_image)) {
    return client_si;
  }
  // Not a native pixmap or unusable in Vulkan; drop it so the caller can fall
  // back to the LINEAR export path.
  sii->DestroySharedImage(gpu::SyncToken(), std::move(client_si));
  return nullptr;
}

scoped_refptr<gpu::ClientSharedImage>
OpenXrGraphicsBindingVulkan::ExportLinearImageAsSharedImage(
    gpu::SharedImageInterface* sii,
    const gpu::SharedImageInfo& si_info,
    const OpenXrVulkanContext::IntermediateImage& image) {
  base::ScopedFD fd = vulkan_->ExportImageMemoryFd(image);
  if (!fd.is_valid()) {
    return nullptr;
  }
  gfx::NativePixmapHandle pixmap_handle;
  pixmap_handle.modifier = 0;  // DRM_FORMAT_MOD_LINEAR
  pixmap_handle.planes.emplace_back(image.stride, image.offset,
                                    image.allocation_size, std::move(fd));
  gfx::GpuMemoryBufferHandle gmb_handle(std::move(pixmap_handle));
  return sii->CreateSharedImage(si_info, std::move(gmb_handle));
}

bool OpenXrGraphicsBindingVulkan::CanUseSharedImages() const {
  return vulkan_ && vulkan_->initialized();
}

bool OpenXrGraphicsBindingVulkan::RequiresSharedImages() const {
  // Linux frames flow exclusively through exported DMA-BUF SharedImages, so
  // the render loop must treat their unavailability as session-ending.
  return true;
}

void OpenXrGraphicsBindingVulkan::CreateSharedImages(
    OpenXrCompositionLayer& layer,
    gpu::SharedImageInterface* sii) {
  CHECK(sii);

  if (!vulkan_ || !vulkan_->valid()) {
    DLOG(ERROR) << __func__ << ": Vulkan not initialized";
    return;
  }

  // VK_KHR_external_memory_fd is required to export VkDeviceMemory as FDs.
  // This extension is not available on SwiftShader, so we skip SharedImage
  // creation in that case — all unit tests take this early-return path.
  if (!vulkan_->has_external_memory_fd()) {
    DVLOG(1) << __func__ << ": VK_KHR_external_memory_fd not available, "
             << "skipping SharedImage creation";
    return;
  }

  const gfx::Size size = layer.GetSwapchainImageSize();
  DVLOG(1) << __func__ << ": SwapchainImageSize=" << size.ToString()
           << " TransferSize=" << layer.GetTransferSize().ToString();
  if (size.IsEmpty()) {
    DLOG(ERROR) << __func__ << ": Swapchain image size is empty";
    return;
  }
  // GetSwapchainFormat() runs during swapchain creation, which always precedes
  // this, and only leaves swapchain_format_ undefined by returning 0 -- which
  // fails swapchain creation, so we would not be here.
  CHECK_NE(swapchain_format_, VK_FORMAT_UNDEFINED);

  // Destroy any previously created intermediate images.
  DestroyIntermediateImages();

  // Match the SharedImage channel order to the negotiated swapchain format so
  // colors are not swizzled. Both SRGB and UNORM BGRA map to kBGRA_8888.
  const viz::SharedImageFormat si_format =
      (swapchain_format_ == VK_FORMAT_B8G8R8A8_SRGB ||
       swapchain_format_ == VK_FORMAT_B8G8R8A8_UNORM)
          ? viz::SinglePlaneFormat::kBGRA_8888
          : viz::SinglePlaneFormat::kRGBA_8888;
  const gpu::SharedImageUsageSet shared_image_usage =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
      gpu::SHARED_IMAGE_USAGE_GLES2_READ | gpu::SHARED_IMAGE_USAGE_GLES2_WRITE;
  const gpu::SharedImageInfo si_info{si_format, size,
                                     gfx::ColorSpace::CreateSRGB(),
                                     shared_image_usage, "OpenXrVulkanBinding"};
  constexpr VkImageUsageFlags kIntermediateUsage =
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  auto swapchain_images = layer.GetSwapchainImages();
  intermediate_images_.resize(swapchain_images.size());

  // Preferred path: let the GPU process allocate via Ozone/GBM, which applies
  // the driver's modifier validation. SCANOUT usage forces a renderable
  // allocation; the returned DMA-BUF is imported for the RenderLayer blit.
  bool use_gbm = true;
  for (size_t i = 0; i < swapchain_images.size(); ++i) {
    scoped_refptr<gpu::ClientSharedImage> client_si =
        ImportGbmImageAsSharedImage(sii, si_info, size, swapchain_format_,
                                    kIntermediateUsage,
                                    &intermediate_images_[i]);
    if (!client_si) {
      // All slots must share one allocation path; restart with LINEAR.
      DVLOG(1) << __func__ << ": slot " << i
               << " GBM path unusable; falling back to LINEAR export";
      DestroyIntermediateImages();
      intermediate_images_.resize(swapchain_images.size());
      for (size_t j = 0; j < i; ++j) {
        swapchain_images[j].shared_image.reset();
        swapchain_images[j].sync_token = gpu::SyncToken();
      }
      use_gbm = false;
      break;
    }
    DVLOG(1) << __func__ << ": slot " << i << " via GBM native pixmap";
    swapchain_images[i].shared_image = std::move(client_si);
    swapchain_images[i].sync_token = sii->GenVerifiedSyncToken();
  }
  if (use_gbm) {
    return;
  }

  // Fallback: allocate LINEAR images in Vulkan and export their DMA-BUFs; the
  // only path when no GBM device exists. No SHARED_IMAGE_USAGE_SCANOUT here:
  // no backing factory satisfies SCANOUT for a modifier-0 NativePixmap.
  for (size_t i = 0; i < swapchain_images.size(); ++i) {
    if (!vulkan_->CreateExportableImage(size, swapchain_format_,
                                        kIntermediateUsage,
                                        &intermediate_images_[i])) {
      DLOG(ERROR) << "Failed to create intermediate image " << i;
      DestroyIntermediateImages();
      return;
    }
    swapchain_images[i].shared_image =
        ExportLinearImageAsSharedImage(sii, si_info, intermediate_images_[i]);
    if (!swapchain_images[i].shared_image) {
      DLOG(ERROR) << "LINEAR export failed for slot " << i;
      DestroyIntermediateImages();
      return;
    }
    swapchain_images[i].sync_token = sii->GenVerifiedSyncToken();
  }
}

bool OpenXrGraphicsBindingVulkan::WaitOnFence(OpenXrCompositionLayer& layer,
                                              gfx::GpuFence& gpu_fence) {
  // Extract the sync FD from the GpuFence. Peek() returns a non-owned FD.
  const gfx::GpuFenceHandle& fence_handle = gpu_fence.GetGpuFenceHandle();
  int fence_fd = fence_handle.Peek();
  if (fence_fd < 0) {
    return true;  // No valid FD — nothing to wait on.
  }
  return vulkan_->WaitOnSyncFd(fence_fd);
}

bool OpenXrGraphicsBindingVulkan::RenderLayer(
    OpenXrCompositionLayer& layer,
    const scoped_refptr<viz::ContextProvider>& context_provider) {
  if (!vulkan_->has_external_memory_fd() || intermediate_images_.empty()) {
    return true;  // No intermediate images — nothing to blit.
  }

  // Determine the active swapchain image and its index.
  OpenXrSwapchainInfo* active_info = layer.GetActiveSwapchainImage();
  if (!active_info) {
    return false;
  }

  // Compute the index of the active image within the swapchain images span.
  base::span<OpenXrSwapchainInfo> swapchain_images = layer.GetSwapchainImages();
  size_t active_index =
      static_cast<size_t>(active_info - swapchain_images.data());
  if (active_index >= intermediate_images_.size()) {
    return false;
  }

  OpenXrVulkanContext::IntermediateImage& src =
      intermediate_images_[active_index];
  VkImage dst = active_info->vk_image;
  if (src.image == VK_NULL_HANDLE || dst == VK_NULL_HANDLE) {
    return false;
  }

  return vulkan_->CopyToSwapchainImage(src, dst, layer.GetSwapchainImageSize());
}

void OpenXrGraphicsBindingVulkan::CleanupWithoutSubmit() {
  // Nothing to do (like the Android binding): this fires on every frame with
  // no submittable content, so it may only drop per-frame state and we keep
  // none. The intermediate images are 1:1 with the swapchain, not per-frame.
}

gfx::Size OpenXrGraphicsBindingVulkan::GetMaxTextureSize() {
  // Queried during session setup (InitializeViewConfig), which runs before the
  // graphics binding's Initialize() creates the Vulkan context. Fall back to
  // the conservative default until the real VkPhysicalDevice limit is known.
  return vulkan_ ? vulkan_->max_texture_size() : gfx::Size(4096, 4096);
}

bool OpenXrGraphicsBindingVulkan::SetOverlayTexture(
    gfx::GpuMemoryBufferHandle texture,
    const gpu::SyncToken& sync_token,
    const gfx::RectF& left,
    const gfx::RectF& right) {
  // The in-headset browser overlay is not wired up on Linux yet (it lands in a
  // follow-up CL), so nothing calls this and the required override is a no-op.
  return true;
}

void OpenXrGraphicsBindingVulkan::OnSwapchainImageReady(
    OpenXrCompositionLayer& layer,
    gpu::SharedImageInterface* sii) {
  // No-op for direct-write: the active swapchain image is already ready.
}

bool OpenXrGraphicsBindingVulkan::SupportsLayers() const {
  return false;
}

void OpenXrGraphicsBindingVulkan::ResizeSharedBuffer(
    OpenXrCompositionLayer& layer,
    OpenXrSwapchainInfo& swap_chain_info,
    gpu::SharedImageInterface* sii) {
  // Re-create all shared images for the layer at the new size.
  DestroyIntermediateImages();
  CreateSharedImages(layer, sii);
}

bool OpenXrGraphicsBindingVulkan::ShouldFlipSubmittedImage(
    OpenXrCompositionLayer& layer) const {
  // GL has bottom-left origin; Vulkan/OpenXR has top-left origin.
  // The submitted image needs to be flipped vertically.
  return true;
}

std::unique_ptr<OpenXrCompositionLayer::GraphicsBindingData>
OpenXrGraphicsBindingVulkan::CreateLayerGraphicsBindingData() const {
  // No per-layer Vulkan state needed for the single-layer MVP.
  return nullptr;
}

}  // namespace device
