// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/angle_vulkan_image_backing_factory.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/gl_image_backing.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing_helper.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
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

using ScopedRestoreTexture = GLTextureImageBackingHelper::ScopedRestoreTexture;

class AngleVulkanImageBacking : public ClearTrackingSharedImageBacking,
                                public GLTextureImageRepresentationClient {
 public:
  AngleVulkanImageBacking(const raw_ptr<SharedContextState>& context_state,
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

  ~AngleVulkanImageBacking() override {
    DCHECK(!is_gl_write_in_process_);
    DCHECK(!is_skia_write_in_process_);
    DCHECK_EQ(gl_reads_in_process_, 0);
    DCHECK_EQ(skia_reads_in_process_, 0);

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

  bool Initialize(const AngleVulkanImageBackingFactory::FormatInfo& format_info,
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

 protected:
  // SharedImageBacking implementation.
  SharedImageBackingType GetType() const override {
    return SharedImageBackingType::kAngleVulkan;
  }

  bool UploadFromMemory(const SkPixmap& pixmap) override {
    PrepareBackendTexture();
    DCHECK(backend_texture_.isValid());

    bool result = gr_context()->updateBackendTexture(backend_texture_, pixmap);
    DCHECK(result);
    SyncImageLayoutFromBackendTexture();
    return result;
  }

  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override {
    NOTREACHED();
  }

  std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override {
    if (!passthrough_texture_ && !InitializePassthroughTexture())
      return nullptr;

    return std::make_unique<GLTexturePassthroughGLCommonRepresentation>(
        manager, this, this, tracker, passthrough_texture_);
  }

  std::unique_ptr<SkiaImageRepresentation> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

  // GLTextureImageRepresentationClient implementation.
  bool GLTextureImageRepresentationBeginAccess(bool readonly) override {
    if (!readonly) {
      // For GL write access.
      if (is_gl_write_in_process_) {
        LOG(DFATAL) << "The backing is being written by GL";
        return false;
      }
      if (is_skia_write_in_process_) {
        LOG(DFATAL) << "The backing is being written by Skia";
        return false;
      }
      if (gl_reads_in_process_ > 0) {
        LOG(DFATAL) << "The backing is being read by GL";
        return false;
      }
      if (skia_reads_in_process_ > 0) {
        LOG(DFATAL) << "The backing is being read by Skia";
        return false;
      }

      // Need to submit recorded work in skia's command buffer to the GPU.
      // TODO(penghuang): only call submit() if it is necessary.
      gr_context()->submit();

      AcquireTextureANGLE();
      is_gl_write_in_process_ = true;

      return true;
    }

    // For GL read access.
    if (is_gl_write_in_process_) {
      LOG(DFATAL) << "The backing is being written by GL";
      return false;
    }
    if (is_skia_write_in_process_) {
      LOG(DFATAL) << "The backing is being written by Skia";
      return false;
    }
    if (skia_reads_in_process_ > 0) {
      // Support cocurrent read?
      LOG(DFATAL) << "The backing is being read by Skia";
      return false;
    }

    ++gl_reads_in_process_;
    if (gl_reads_in_process_ == 1) {
      // For the first GL access.
      // Need to submit recorded work in skia's command buffer to the GPU.
      // TODO(penghuang): only call submit() if it is necessary.
      gr_context()->submit();

      AcquireTextureANGLE();
    }

    return true;
  }

  void GLTextureImageRepresentationEndAccess(bool readonly) override {
    if (readonly) {
      // For GL read access.
      if (gl_reads_in_process_ == 0) {
        LOG(DFATAL) << "The backing is not being read by GL";
        return;
      }

      --gl_reads_in_process_;

      // For the last GL read access, release texture from ANGLE.
      if (gl_reads_in_process_ == 0)
        ReleaseTextureANGLE();

      return;
    }

    // For GL write access.
    if (!is_gl_write_in_process_) {
      LOG(DFATAL) << "The backing is not being written by GL";
      return;
    }

    is_gl_write_in_process_ = false;
    ReleaseTextureANGLE();
  }

  void GLTextureImageRepresentationRelease(bool have_context) override {}

 private:
  class SkiaAngleVulkanImageRepresentation;

  void AcquireTextureANGLE() {
    gl::GLApi* api = gl::g_current_gl_context;
    GLuint texture = passthrough_texture_->service_id();
    // Acquire the texture, so ANGLE can access it.
    api->glAcquireTexturesANGLEFn(1, &texture, &layout_);
  }

  void ReleaseTextureANGLE() {
    gl::GLApi* api = gl::g_current_gl_context;
    GLuint texture = passthrough_texture_->service_id();
    // Release the texture from ANGLE, so it can be used elsewhere.
    api->glReleaseTexturesANGLEFn(1, &texture, &layout_);
  }

  void PrepareBackendTexture() {
    if (!backend_texture_.isValid()) {
      GrVkImageInfo info = CreateGrVkImageInfo(vulkan_image_.get());
      backend_texture_ =
          GrBackendTexture(size().width(), size().height(), info);
    }
    auto vk_layout = GLImageLayoutToVkImageLayout(layout_);
    backend_texture_.setVkImageLayout(vk_layout);
  }

  void SyncImageLayoutFromBackendTexture() {
    GrVkImageInfo info;
    bool result = backend_texture_.getVkImageInfo(&info);
    DCHECK(result);
    layout_ = VkImageLayoutToGLImageLayout(info.fImageLayout);
  }

  bool BeginAccessSkia(bool readonly) {
    if (!readonly) {
      // Skia write access
      if (is_gl_write_in_process_) {
        LOG(DFATAL) << "The backing is being written by GL";
        return false;
      }
      if (is_skia_write_in_process_) {
        LOG(DFATAL) << "The backing is being written by Skia";
        return false;
      }
      if (gl_reads_in_process_) {
        LOG(DFATAL) << "The backing is being written by GL";
        return false;
      }
      if (skia_reads_in_process_) {
        LOG(DFATAL) << "The backing is being written by Skia";
        return false;
      }
      PrepareBackendTexture();
      is_skia_write_in_process_ = true;
      return true;
    }

    // Skia read access
    if (is_gl_write_in_process_) {
      LOG(DFATAL) << "The backing is being written by GL";
      return false;
    }
    if (is_skia_write_in_process_) {
      LOG(DFATAL) << "The backing is being written by Skia";
      return false;
    }

    if (skia_reads_in_process_ == 0) {
      // The first skia access.
      if (gl_reads_in_process_ == 0) {
      } else {
        if (!gl::GLContext::GetCurrent())
          context_state_->MakeCurrent(/*surface=*/nullptr, /*needs_gl=*/true);
        // Release texture from ANGLE temporarily, so skia can access it.
        // After skia accessing, we will recover GL access.
        ReleaseTextureANGLE();
      }
      PrepareBackendTexture();
    }

    ++skia_reads_in_process_;
    return true;
  }

  void EndAccessSkia() {
    if (skia_reads_in_process_ == 0 && !is_skia_write_in_process_) {
      LOG(DFATAL) << "The backing is not being accessed by Skia.";
      return;
    }

    if (is_skia_write_in_process_) {
      is_skia_write_in_process_ = false;
    } else {
      --skia_reads_in_process_;
      if (skia_reads_in_process_ > 0)
        return;
    }

    SyncImageLayoutFromBackendTexture();

    if (gl_reads_in_process_ > 0) {
      if (!gl::GLContext::GetCurrent())
        context_state_->MakeCurrent(/*surface=*/nullptr, /*needs_gl=*/true);
      // Recover GL access.
      AcquireTextureANGLE();
    }
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
    GLTextureImageBackingHelper::MakeTextureAndSetParameters(
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
    SkPixmap pixmap(AsSkImageInfo(), pixel_data.data(), stride);
    UploadFromMemory(pixmap);
  }

  GrDirectContext* gr_context() { return context_state_->gr_context(); }

  const raw_ptr<SharedContextState> context_state_;
  std::unique_ptr<VulkanImage> vulkan_image_;
  scoped_refptr<gl::GLImageEGLAngleVulkan> egl_image_;
  scoped_refptr<gles2::TexturePassthrough> passthrough_texture_;
  GrBackendTexture backend_texture_{};
  GLenum layout_ = GL_NONE;
  bool is_skia_write_in_process_ = false;
  bool is_gl_write_in_process_ = false;
  int skia_reads_in_process_ = 0;
  int gl_reads_in_process_ = 0;
};

class AngleVulkanImageBacking::SkiaAngleVulkanImageRepresentation
    : public SkiaImageRepresentation {
 public:
  SkiaAngleVulkanImageRepresentation(SharedImageManager* manager,
                                     AngleVulkanImageBacking* backing,
                                     MemoryTypeTracker* tracker)
      : SkiaImageRepresentation(manager, backing, tracker) {}

  ~SkiaAngleVulkanImageRepresentation() override {
    backing_impl()->context_state_->EraseCachedSkSurface(this);
  }

  // SkiaImageRepresentation implementation.
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
  AngleVulkanImageBacking* backing_impl() const {
    return static_cast<AngleVulkanImageBacking*>(backing());
  }

  int surface_msaa_count_ = 0;
};

std::unique_ptr<SkiaImageRepresentation> AngleVulkanImageBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  DCHECK_EQ(context_state_, context_state.get());
  return std::make_unique<SkiaAngleVulkanImageRepresentation>(manager, this,
                                                              tracker);
}

}  // namespace

AngleVulkanImageBackingFactory::AngleVulkanImageBackingFactory(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    SharedContextState* context_state)
    : GLCommonImageBackingFactory(gpu_preferences,
                                  workarounds,
                                  context_state->feature_info(),
                                  context_state->progress_reporter()),
      context_state_(context_state) {
  DCHECK(context_state_->GrContextIsVulkan());
  DCHECK(gl::GLSurfaceEGL::GetGLDisplayEGL()->ext->b_EGL_ANGLE_vulkan_image);
}

AngleVulkanImageBackingFactory::~AngleVulkanImageBackingFactory() = default;

std::unique_ptr<SharedImageBacking>
AngleVulkanImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    bool is_thread_safe) {
  auto backing = std::make_unique<AngleVulkanImageBacking>(
      context_state_, mailbox, format, size, color_space, surface_origin,
      alpha_type, usage);

  if (!backing->Initialize(format_info_[format], {}))
    return nullptr;

  return backing;
}

std::unique_ptr<SharedImageBacking>
AngleVulkanImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::span<const uint8_t> data) {
  auto backing = std::make_unique<AngleVulkanImageBacking>(
      context_state_, mailbox, format, size, color_space, surface_origin,
      alpha_type, usage);

  if (!backing->Initialize(format_info_[format], data))
    return nullptr;

  return backing;
}

std::unique_ptr<SharedImageBacking>
AngleVulkanImageBackingFactory::CreateSharedImage(
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
  NOTREACHED();
  return nullptr;
}

bool AngleVulkanImageBackingFactory::CanUseAngleVulkanImageBacking(
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
      SHARED_IMAGE_USAGE_OOP_RASTERIZATION | SHARED_IMAGE_USAGE_CPU_UPLOAD;

  if (usage & ~kSupportedUsages)
    return false;

  // AngleVulkan backing is used for GL & Vulkan interop, so the usage must
  // contain GLES2
  // TODO(penghuang): use AngleVulkan backing for non GL & Vulkan interop usage?
  return usage & SHARED_IMAGE_USAGE_GLES2;
}

bool AngleVulkanImageBackingFactory::IsSupported(
    uint32_t usage,
    viz::ResourceFormat format,
    const gfx::Size& size,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    base::span<const uint8_t> pixel_data) {
  DCHECK_EQ(gr_context_type, GrContextType::kVulkan);
  if (!CanUseAngleVulkanImageBacking(usage))
    return false;

  if (thread_safe)
    return false;

  if (gmb_type != gfx::EMPTY_BUFFER) {
    return false;
  }

  return CanCreateSharedImage(size, pixel_data, format_info_[format],
                              GL_TEXTURE_2D);
}

}  // namespace gpu
