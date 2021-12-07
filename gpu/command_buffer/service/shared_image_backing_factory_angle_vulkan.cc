// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_angle_vulkan.h"

#include "base/logging.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_backing_gl_common.h"
#include "gpu/command_buffer/service/shared_image_backing_gl_image.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_util.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_egl_angle_vulkan.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/progress_reporter.h"

namespace gpu {

namespace {

size_t EstimatedSize(viz::ResourceFormat format, const gfx::Size& size) {
  size_t estimated_size = 0;
  viz::ResourceSizes::MaybeSizeInBytes(size, format, &estimated_size);
  return estimated_size;
}

GrVkImageInfo CreateGrVkImageInfo(
    VkImage vk_image,
    const VkImageCreateInfo& info,
    const raw_ptr<SharedContextState>& context_state) {
  DCHECK_NE(vk_image, VK_NULL_HANDLE);

  bool is_protected = info.flags & VK_IMAGE_CREATE_PROTECTED_BIT;

  GrVkImageInfo image_info;
  image_info.fImage = vk_image;
  image_info.fAlloc = {};
  image_info.fImageTiling = info.tiling;
  image_info.fImageLayout = info.initialLayout;
  image_info.fFormat = info.format;
  image_info.fImageUsageFlags = info.usage;
  image_info.fSampleCount = info.samples;
  image_info.fLevelCount = info.mipLevels;
  image_info.fCurrentQueueFamily = context_state->vk_context_provider()
                                       ->GetDeviceQueue()
                                       ->GetVulkanQueueIndex();
  image_info.fProtected = is_protected ? GrProtected::kYes : GrProtected::kNo;
  image_info.fYcbcrConversionInfo = {};

  return image_info;
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
    if (!passthrough_texture_)
      return;

    if (!have_context() ||
        !context_state_->MakeCurrent(/*surface=*/nullptr, /*needs_gl=*/true))
      passthrough_texture_->MarkContextLost();

    passthrough_texture_.reset();
  }

  bool Initialize(
      const SharedImageBackingFactoryAngleVulkan::FormatInfo& format_info,
      const SharedImageBackingGLCommon::UnpackStateAttribs& attribs) {
    SharedImageBackingGLCommon::MakeTextureAndSetParameters(
        GL_TEXTURE_2D, /*service_id=*/0, /*framebuffer_attachment_angle=*/true,
        &passthrough_texture_, nullptr);
    passthrough_texture_->SetEstimatedSize(estimated_size());

    GLuint texture = passthrough_texture_->service_id();

    gl::GLApi* api = gl::g_current_gl_context;
    ScopedRestoreTexture scoped_restore(api, GL_TEXTURE_2D);
    api->glBindTextureFn(GL_TEXTURE_2D, texture);

    if (format_info.supports_storage) {
      {
        gl::ScopedProgressReporter scoped_progress_reporter(
            context_state_->progress_reporter());
        api->glTexStorage2DEXTFn(GL_TEXTURE_2D, 1,
                                 format_info.storage_internal_format,
                                 size().width(), size().height());
      }

    } else {
      ScopedResetAndRestoreUnpackState scoped_unpack_state(api, attribs, false);
      gl::ScopedProgressReporter scoped_progress_reporter(
          context_state_->progress_reporter());
      api->glTexImage2DFn(GL_TEXTURE_2D, 0, format_info.image_internal_format,
                          size().width(), size().height(), 0,
                          format_info.adjusted_format, format_info.gl_type,
                          nullptr);
    }

    if (gl::g_current_gl_driver->ext.b_GL_KHR_debug) {
      const std::string label =
          "SharedImage_AngleVulkan" + CreateLabelForSharedImageUsage(usage());
      api->glObjectLabelFn(GL_TEXTURE, texture, -1, label.c_str());
    }

    // Release the texture from ANGLE.
    api->glReleaseTexturesANGLEFn(1, &texture, &layout_);

    auto image = base::MakeRefCounted<gl::GLImageEGLAngleVulkan>(size());
    if (!image->Initialize(texture)) {
      passthrough_texture_.reset();
      return false;
    }
    image_ = std::move(image);
    return true;
  }

 protected:
  // SharedImageBacking implementation.
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override {
    NOTREACHED() << "Not supported.";
    return false;
  }

  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override {
    NOTREACHED() << "Not supported.";
  }

  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override {}

  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override {
    DCHECK(passthrough_texture_);
    return std::make_unique<SharedImageRepresentationGLTexturePassthroughImpl>(
        manager, this, this, tracker, passthrough_texture_);
  }

  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

  // SharedImageRepresentationGLTextureClient implementation.
  bool SharedImageRepresentationGLTextureBeginAccess() override {
    gl::GLApi* api = gl::g_current_gl_context;
    GLuint texture = passthrough_texture_->service_id();
    // Acquire the texture, so ANGLE can access it.
    api->glAcquireTexturesANGLEFn(1, &texture, &layout_);
    return true;
  }

  void SharedImageRepresentationGLTextureEndAccess(bool readonly) override {
    gl::GLApi* api = gl::g_current_gl_context;
    GLuint texture = passthrough_texture_->service_id();
    // Release the texture from ANGLE, so it can be used elsewhere.
    api->glReleaseTexturesANGLEFn(1, &texture, &layout_);
  }

  void SharedImageRepresentationGLTextureRelease(bool have_context) override {
    if (!have_context) {
      passthrough_texture_->MarkContextLost();
    }
    passthrough_texture_.reset();
    image_.reset();
  }

 private:
  class SkiaRepresentation;

  bool BeginAccessSkia() {
    VkImageCreateInfo info;
    VkImage vk_image = image_->ExportVkImage(&info);
    // Check whether VkImage is re-created in ANGLE.
    if (vk_image != vk_image_) {
      vk_image_ = vk_image;
      backend_texture_ = GrBackendTexture(
          size().width(), size().height(),
          CreateGrVkImageInfo(vk_image_, info, context_state_));
    }
    auto vk_layout = GLImageLayoutToVkImageLayout(layout_);
    backend_texture_.setVkImageLayout(vk_layout);
    return true;
  }

  void EndAccessSkia() {
    GrVkImageInfo info;
    bool result = backend_texture_.getVkImageInfo(&info);
    DCHECK(result);
    layout_ = VkImageLayoutToGLImageLayout(info.fImageLayout);
  }

  raw_ptr<SharedContextState> context_state_;
  scoped_refptr<gl::GLImageEGLAngleVulkan> image_;
  scoped_refptr<gles2::TexturePassthrough> passthrough_texture_;
  GrBackendTexture backend_texture_{};
  VkImage vk_image_ = VK_NULL_HANDLE;
  GLenum layout_ = GL_NONE;
};  // namespace

class AngleVulkanBacking::SkiaRepresentation
    : public SharedImageRepresentationSkia {
 public:
  SkiaRepresentation(SharedImageManager* manager,
                     AngleVulkanBacking* backing,
                     MemoryTypeTracker* tracker)
      : SharedImageRepresentationSkia(manager, backing, tracker) {}

  ~SkiaRepresentation() override = default;

  // SharedImageRepresentationSkia implementation.
  sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    if (!backing_impl()->BeginAccessSkia())
      return nullptr;
    return SkPromiseImageTexture::Make(backing_impl()->backend_texture_);
  }

  void EndReadAccess() override { backing_impl()->EndAccessSkia(); }

  sk_sp<SkPromiseImageTexture> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    // TODO(penghuang): support it for OOP-C.
    NOTIMPLEMENTED();
    return nullptr;
  }
  void EndWriteAccess(sk_sp<SkSurface> surface) override {}

 private:
  AngleVulkanBacking* backing_impl() const {
    return static_cast<AngleVulkanBacking*>(backing());
  }

  sk_sp<SkPromiseImageTexture> BeginAccess(
      bool readonly,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores);

  void EndAccess(bool readonly);
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
    const GpuFeatureInfo& gpu_feature_info,
    SharedContextState* context_state)
    : SharedImageBackingFactoryGLCommon(gpu_preferences,
                                        workarounds,
                                        gpu_feature_info,
                                        context_state->progress_reporter()),
      context_state_(context_state) {
  DCHECK(gl::GLSurfaceEGL::IsANGLEVulkanImageClientBufferSupported());
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

  auto result = std::make_unique<AngleVulkanBacking>(
      context_state_, mailbox, format, size, color_space, surface_origin,
      alpha_type, usage);

  if (!result->Initialize(format_info, attribs_))
    return nullptr;
  return result;
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
  NOTREACHED() << "Not supported";
  return nullptr;
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
  NOTREACHED() << "Not supported";
  return nullptr;
}

bool SharedImageBackingFactoryAngleVulkan::CanUseAngleVulkanBacking(
    uint32_t usage,
    GrContextType gr_context_type) const {
  // Ignore for mipmap usage.
  usage &= ~SHARED_IMAGE_USAGE_MIPMAP;

  constexpr auto kSupportedUsages =
      SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT |
      SHARED_IMAGE_USAGE_RASTER | SHARED_IMAGE_USAGE_DISPLAY |
      SHARED_IMAGE_USAGE_OOP_RASTERIZATION;

  if (usage & ~kSupportedUsages)
    return false;

  // AngleVulkan backing is used for GL & Vulkan interop, so the usage must
  // contain GLES2
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
  if (!CanUseAngleVulkanBacking(usage, gr_context_type)) {
    return false;
  }

  if (is_pixel_used) {
    return false;
  }

  if (gmb_type != gfx::EMPTY_BUFFER) {
    return false;
  }

  *allow_legacy_mailbox = false;
  return true;
}

}  // namespace gpu
