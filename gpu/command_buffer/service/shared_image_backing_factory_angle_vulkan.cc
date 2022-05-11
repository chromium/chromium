// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_angle_vulkan.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_backing_gl_common.h"
#include "gpu/command_buffer/service/shared_image_backing_gl_image.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_util.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_egl_angle_vulkan.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/progress_reporter.h"
#include "ui/gl/trace_util.h"

namespace gpu {

namespace {

size_t EstimatedSize(viz::ResourceFormat format, const gfx::Size& size) {
  size_t estimated_size = 0;
  viz::ResourceSizes::MaybeSizeInBytes(size, format, &estimated_size);
  return estimated_size;
}

using ScopedResetAndRestoreUnpackState =
    SharedImageBackingGLCommon::ScopedResetAndRestoreUnpackState;

using ScopedRestoreTexture = SharedImageBackingGLCommon::ScopedRestoreTexture;

class AngleVulkanBacking : public ClearTrackingSharedImageBacking,
                           public SharedImageRepresentationGLTextureClient {
 public:
  AngleVulkanBacking(const raw_ptr<SharedContextState>& context_state,
                     const Mailbox& mailbox,
                     viz::ResourceFormat format,
                     const gfx::Size& size,
                     const gfx::ColorSpace& color_space,
                     GrSurfaceOrigin surface_origin,
                     SkAlphaType alpha_type,
                     uint32_t usage)
      : ClearTrackingSharedImageBacking(mailbox,
                                        format,
                                        size,
                                        color_space,
                                        surface_origin,
                                        alpha_type,
                                        usage,
                                        EstimatedSize(format, size),
                                        false /* is_thread_safe */),
        context_state_(context_state) {}

  ~AngleVulkanBacking() override {
    DCHECK_EQ(access_mode_, kNone);
    if (passthrough_texture_) {
      if (!gl::GLContext::GetCurrent())
        context_state_->MakeCurrent(/*surface=*/nullptr, /*needs_gl=*/true);

      if (!have_context())
        passthrough_texture_->MarkContextLost();

      passthrough_texture_.reset();
      egl_image_.reset();
    }
    if (vulkan_image_) {
      auto* fence_helper = context_state_->vk_context_provider()
                               ->GetDeviceQueue()
                               ->GetFenceHelper();
      fence_helper->EnqueueVulkanObjectCleanupForSubmittedWork(
          std::move(vulkan_image_));
    }
  }

  bool Initialize(
      const SharedImageBackingFactoryAngleVulkan::FormatInfo& format_info,
      const base::span<const uint8_t>& data) {
    auto* device_queue =
        context_state_->vk_context_provider()->GetDeviceQueue();
    VkFormat vk_format = ToVkFormat(format());

    constexpr auto kUsageNeedsColorAttachment =
        SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT |
        SHARED_IMAGE_USAGE_RASTER | SHARED_IMAGE_USAGE_OOP_RASTERIZATION |
        SHARED_IMAGE_USAGE_WEBGPU;
    VkImageUsageFlags vk_usage = VK_IMAGE_USAGE_SAMPLED_BIT |
                                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                 VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (usage() & kUsageNeedsColorAttachment) {
      vk_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                  VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
      if (format() == viz::ETC1) {
        DLOG(ERROR) << "ETC1 format cannot be used as color attachment.";
        return false;
      }
    }

    VkImageCreateFlags vk_flags = 0;
    auto vulkan_image =
        VulkanImage::Create(device_queue, size(), vk_format, vk_usage, vk_flags,
                            VK_IMAGE_TILING_OPTIMAL);

    if (!vulkan_image)
      return false;

    vulkan_image_ = std::move(vulkan_image);

    if (!data.empty()) {
      size_t stride = BitsPerPixel(format()) / 8 * size().width();
      WritePixels(data, stride);
      SetCleared();
    }

    return true;
  }

  bool InitializeFromGMB(
      const SharedImageBackingFactoryAngleVulkan::FormatInfo& format_info,
      gfx::GpuMemoryBufferHandle handle) {
    SharedMemoryRegionWrapper shared_memory_wrapper;
    if (!shared_memory_wrapper.Initialize(handle, size(), format()))
      return false;

    if (!Initialize(format_info, {}))
      return false;

    shared_memory_wrapper_ = std::move(shared_memory_wrapper);
    Update(nullptr);
    SetCleared();

    return true;
  }

 protected:
  // SharedImageBacking implementation.
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override {
    NOTREACHED() << "Not supported.";
    return false;
  }

  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override {
    DCHECK(!in_fence);
    need_copy_pixels_from_shm_ = true;
  }

  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override {
    if (auto tracing_id = GrBackendTextureTracingID(backend_texture_)) {
      // Add a |service_guid| which expresses shared ownership between the
      // various GPU dumps.
      auto client_guid = GetSharedImageGUIDForTracing(mailbox());
      auto service_guid = gl::GetGLTextureServiceGUIDForTracing(tracing_id);
      pmd->CreateSharedGlobalAllocatorDump(service_guid);

      std::string format_dump_name =
          base::StringPrintf("%s/format=%d", dump_name.c_str(), format());
      base::trace_event::MemoryAllocatorDump* format_dump =
          pmd->CreateAllocatorDump(format_dump_name);
      format_dump->AddScalar(
          base::trace_event::MemoryAllocatorDump::kNameSize,
          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
          static_cast<uint64_t>(EstimatedSizeForMemTracking()));

      int importance = 2;  // This client always owns the ref.
      pmd->AddOwnershipEdge(client_guid, service_guid, importance);
    }
  }

  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override {
    if (!passthrough_texture_ && !InitializePassthroughTexture())
      return nullptr;

    return std::make_unique<SharedImageRepresentationGLTexturePassthroughImpl>(
        manager, this, this, tracker, passthrough_texture_);
  }

  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

  // SharedImageRepresentationGLTextureClient implementation.
  bool SharedImageRepresentationGLTextureBeginAccess() override {
    if (access_mode_ != kNone) {
      LOG(DFATAL) << "The backing is being accessed with mode:" << access_mode_;
      return false;
    }

    // Sync pixels data from SHM to VkImage
    CopyPixelsFromSHMIfNecessary();

    access_mode_ = kGLReadWrite;

    // Need to submit recorded work in skia's command buffer to the GPU.
    // TODO(penghuang): only call submit() if it is necessary.
    gr_context()->submit();

    gl::GLApi* api = gl::g_current_gl_context;
    GLuint texture = passthrough_texture_->service_id();
    // Acquire the texture, so ANGLE can access it.
    api->glAcquireTexturesANGLEFn(1, &texture, &layout_);
    return true;
  }

  void SharedImageRepresentationGLTextureEndAccess(bool readonly) override {
    if (access_mode_ != kGLReadWrite) {
      LOG(DFATAL) << "The backing is not being accessed by GL. mode:"
                  << access_mode_;
      return;
    }
    access_mode_ = kNone;

    gl::GLApi* api = gl::g_current_gl_context;
    GLuint texture = passthrough_texture_->service_id();
    // Release the texture from ANGLE, so it can be used elsewhere.
    api->glReleaseTexturesANGLEFn(1, &texture, &layout_);
  }

  void SharedImageRepresentationGLTextureRelease(bool have_context) override {}

 private:
  class SkiaRepresentation;

  bool BeginAccessSkia(bool readonly) {
    if (access_mode_ != kNone) {
      LOG(DFATAL) << "The backing is being accessed with mode:" << access_mode_;
      return false;
    }

    // Sync pixels data from SHM to VkImage
    CopyPixelsFromSHMIfNecessary();

    access_mode_ = readonly ? kSkiaReadOnly : kSkiaReadWrite;

    if (!backend_texture_.isValid()) {
      GrVkImageInfo info = CreateGrVkImageInfo(vulkan_image_.get());
      backend_texture_ =
          GrBackendTexture(size().width(), size().height(), info);
    }
    auto vk_layout = GLImageLayoutToVkImageLayout(layout_);
    backend_texture_.setVkImageLayout(vk_layout);
    return true;
  }

  void EndAccessSkia() {
    if (access_mode_ != kSkiaReadOnly && access_mode_ != kSkiaReadWrite) {
      LOG(DFATAL) << "The backing is not being accessed by Skia. mode:"
                  << access_mode_;
      return;
    }
    access_mode_ = kNone;

    GrVkImageInfo info;
    bool result = backend_texture_.getVkImageInfo(&info);
    DCHECK(result);
    layout_ = VkImageLayoutToGLImageLayout(info.fImageLayout);
  }

  bool InitializePassthroughTexture() {
    DCHECK(vulkan_image_);
    DCHECK(!egl_image_);
    DCHECK(!passthrough_texture_);

    auto egl_image = base::MakeRefCounted<gl::GLImageEGLAngleVulkan>(size());
    if (!egl_image->Initialize(vulkan_image_->image(),
                               &vulkan_image_->create_info(),
                               viz::GLInternalFormat(format()))) {
      return false;
    }

    scoped_refptr<gles2::TexturePassthrough> passthrough_texture;
    SharedImageBackingGLCommon::MakeTextureAndSetParameters(
        GL_TEXTURE_2D, /*service_id=*/0,
        /*framebuffer_attachment_angle=*/true, &passthrough_texture, nullptr);
    passthrough_texture->SetEstimatedSize(estimated_size());

    GLuint texture = passthrough_texture->service_id();

    gl::GLApi* api = gl::g_current_gl_context;
    ScopedRestoreTexture scoped_restore(api, GL_TEXTURE_2D);
    api->glBindTextureFn(GL_TEXTURE_2D, texture);

    if (!egl_image->BindTexImage(GL_TEXTURE_2D))
      return false;

    if (gl::g_current_gl_driver->ext.b_GL_KHR_debug) {
      const std::string label =
          "SharedImage_AngleVulkan" + CreateLabelForSharedImageUsage(usage());
      api->glObjectLabelFn(GL_TEXTURE, texture, label.size(), label.c_str());
    }

    egl_image_ = std::move(egl_image);
    passthrough_texture_ = std::move(passthrough_texture);

    return true;
  }

  void WritePixels(const base::span<const uint8_t>& pixel_data, size_t stride) {
    bool result = BeginAccessSkia(/*readonly=*/false);
    DCHECK(result);
    DCHECK(backend_texture_.isValid());

    auto info = SkImageInfo::Make(size().width(), size().height(),
                                  ResourceFormatToClosestSkColorType(
                                      /*gpu_compositing=*/true, format()),
                                  kOpaque_SkAlphaType);
    SkPixmap pixmap(info, pixel_data.data(), stride);
    result = gr_context()->updateBackendTexture(backend_texture_, pixmap);
    DCHECK(result);

    EndAccessSkia();
  }

  void CopyPixelsFromSHMIfNecessary() {
    if (need_copy_pixels_from_shm_) {
      // Set need_copy_pixels_from_shm_ to false before call WritePixels(),
      // because it will call BeginAccessSkia() which will call
      // CopyPixelsFromSHMIfNecessary() again.
      need_copy_pixels_from_shm_ = false;
      WritePixels(shared_memory_wrapper_.GetMemoryAsSpan(),
                  shared_memory_wrapper_.GetStride());
    }
  }

  GrDirectContext* gr_context() { return context_state_->gr_context(); }

  const raw_ptr<SharedContextState> context_state_;
  std::unique_ptr<VulkanImage> vulkan_image_;
  scoped_refptr<gl::GLImageEGLAngleVulkan> egl_image_;
  scoped_refptr<gles2::TexturePassthrough> passthrough_texture_;
  GrBackendTexture backend_texture_{};
  GLenum layout_ = GL_NONE;
  enum AccessMode {
    kNone,
    kSkiaReadOnly,
    kSkiaReadWrite,
    kGLReadWrite,
  };
  AccessMode access_mode_ = kNone;
  SharedMemoryRegionWrapper shared_memory_wrapper_;
  bool need_copy_pixels_from_shm_ = false;
};

class AngleVulkanBacking::SkiaRepresentation
    : public SharedImageRepresentationSkia {
 public:
  SkiaRepresentation(SharedImageManager* manager,
                     AngleVulkanBacking* backing,
                     MemoryTypeTracker* tracker)
      : SharedImageRepresentationSkia(manager, backing, tracker) {}

  ~SkiaRepresentation() override {
    backing_impl()->context_state_->EraseCachedSkSurface(this);
  }

  // SharedImageRepresentationSkia implementation.
  sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    if (!backing_impl()->BeginAccessSkia(/*readonly=*/true))
      return nullptr;
    return SkPromiseImageTexture::Make(backing_impl()->backend_texture_);
  }

  void EndReadAccess() override { backing_impl()->EndAccessSkia(); }

  sk_sp<SkPromiseImageTexture> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    if (!backing_impl()->BeginAccessSkia(/*readonly=*/false))
      return nullptr;
    return SkPromiseImageTexture::Make(backing_impl()->backend_texture_);
  }

  sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    auto promise_image_texture =
        BeginWriteAccess(begin_semaphores, end_semaphores, end_state);
    if (!promise_image_texture)
      return nullptr;

    auto surface = backing_impl()->context_state_->GetCachedSkSurface(this);

    // If surface properties are different from the last access, then we cannot
    // reuse the cached SkSurface.
    if (!surface || surface_props != surface->props() ||
        final_msaa_count != surface_msaa_count_) {
      SkColorType sk_color_type = viz::ResourceFormatToClosestSkColorType(
          true /* gpu_compositing */, format());
      surface = SkSurface::MakeFromBackendTexture(
          backing_impl()->gr_context(), promise_image_texture->backendTexture(),
          surface_origin(), final_msaa_count, sk_color_type,
          backing_impl()->color_space().ToSkColorSpace(), &surface_props);
      if (!surface) {
        backing_impl()->context_state_->EraseCachedSkSurface(this);
        return nullptr;
      }
      surface_msaa_count_ = final_msaa_count;
      backing_impl()->context_state_->CacheSkSurface(this, surface);
    }

    [[maybe_unused]] int count = surface->getCanvas()->save();
    DCHECK_EQ(count, 1);

    return surface;
  }

  void EndWriteAccess(sk_sp<SkSurface> surface) override {
    if (surface) {
      surface->getCanvas()->restoreToCount(1);
      surface = nullptr;
      DCHECK(backing_impl()->context_state_->CachedSkSurfaceIsUnique(this));
    }
    backing_impl()->EndAccessSkia();
  }

 private:
  AngleVulkanBacking* backing_impl() const {
    return static_cast<AngleVulkanBacking*>(backing());
  }

  int surface_msaa_count_ = 0;
};

std::unique_ptr<SharedImageRepresentationSkia> AngleVulkanBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  DCHECK_EQ(context_state_, context_state.get());
  return std::make_unique<SkiaRepresentation>(manager, this, tracker);
}

}  // namespace

SharedImageBackingFactoryAngleVulkan::SharedImageBackingFactoryAngleVulkan(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    SharedContextState* context_state)
    : SharedImageBackingFactoryGLCommon(gpu_preferences,
                                        workarounds,
                                        context_state->feature_info(),
                                        context_state->progress_reporter()),
      context_state_(context_state) {
  DCHECK(context_state_->GrContextIsVulkan());
  DCHECK(gl::GLSurfaceEGL::GetGLDisplayEGL()->IsANGLEVulkanImageSupported());
}

SharedImageBackingFactoryAngleVulkan::~SharedImageBackingFactoryAngleVulkan() =
    default;

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryAngleVulkan::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    bool is_thread_safe) {
  const FormatInfo& format_info = format_info_[format];
  if (!CanCreateSharedImage(size, /*pixel_data=*/{}, format_info,
                            GL_TEXTURE_2D)) {
    return nullptr;
  }

  auto backing = std::make_unique<AngleVulkanBacking>(
      context_state_, mailbox, format, size, color_space, surface_origin,
      alpha_type, usage);

  if (!backing->Initialize(format_info, {}))
    return nullptr;

  return backing;
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryAngleVulkan::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::span<const uint8_t> data) {
  const FormatInfo& format_info = format_info_[format];
  if (!CanCreateSharedImage(size, data, format_info, GL_TEXTURE_2D))
    return nullptr;

  auto backing = std::make_unique<AngleVulkanBacking>(
      context_state_, mailbox, format, size, color_space, surface_origin,
      alpha_type, usage);

  if (!backing->Initialize(format_info, data))
    return nullptr;

  return backing;
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryAngleVulkan::CreateSharedImage(
    const Mailbox& mailbox,
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    gfx::BufferPlane plane,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  if (plane != gfx::BufferPlane::DEFAULT) {
    LOG(DFATAL) << "Invalid plane";
    return nullptr;
  }

  if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(size, buffer_format)) {
    LOG(DFATAL) << "Invalid image size for format.";
    return nullptr;
  }

  auto format = viz::GetResourceFormat(buffer_format);

  const FormatInfo& format_info = format_info_[format];
  if (!CanCreateSharedImage(size, /*pixel_data=*/{}, format_info,
                            GL_TEXTURE_2D)) {
    return nullptr;
  }

  auto backing = std::make_unique<AngleVulkanBacking>(
      context_state_, mailbox, format, size, color_space, surface_origin,
      alpha_type, usage);

  if (!backing->InitializeFromGMB(format_info, std::move(handle)))
    return nullptr;

  return backing;
}

bool SharedImageBackingFactoryAngleVulkan::CanUseAngleVulkanBacking(
    uint32_t usage) const {
  // Ignore for mipmap usage.
  usage &= ~SHARED_IMAGE_USAGE_MIPMAP;

  // TODO(penghuang): verify the scanout is the right usage for video playback.
  // crbug.com/1280798
  constexpr auto kSupportedUsages =
#if BUILDFLAG(IS_LINUX)
      SHARED_IMAGE_USAGE_SCANOUT |
#endif
      SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT |
      SHARED_IMAGE_USAGE_RASTER | SHARED_IMAGE_USAGE_DISPLAY |
      SHARED_IMAGE_USAGE_OOP_RASTERIZATION;

  if (usage & ~kSupportedUsages)
    return false;

  // AngleVulkan backing is used for GL & Vulkan interop, so the usage must
  // contain GLES2
  // TODO(penghuang): use AngleVulkan backing for non GL & Vulkan interop usage?
  return usage & SHARED_IMAGE_USAGE_GLES2;
}

bool SharedImageBackingFactoryAngleVulkan::IsSupported(
    uint32_t usage,
    viz::ResourceFormat format,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    bool* allow_legacy_mailbox,
    bool is_pixel_used) {
  DCHECK_EQ(gr_context_type, GrContextType::kVulkan);
  if (!CanUseAngleVulkanBacking(usage))
    return false;

  if (thread_safe)
    return false;

  switch (gmb_type) {
    case gfx::EMPTY_BUFFER:
    case gfx::SHARED_MEMORY_BUFFER:
      break;
    default:
      return false;
  }

  *allow_legacy_mailbox = false;

  return true;
}

}  // namespace gpu
