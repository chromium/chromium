// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/angle_vulkan_image_backing.h"

#include "base/logging.h"
#include "base/trace_event/process_memory_dump.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing_helper.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_util.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_egl_angle_vulkan.h"
#include "ui/gl/gl_surface_egl.h"

namespace gpu {

namespace {

using ScopedRestoreTexture = GLTextureImageBackingHelper::ScopedRestoreTexture;

}  // namespace

class AngleVulkanImageBacking::SkiaAngleVulkanImageRepresentation
    : public SkiaImageRepresentation {
 public:
  SkiaAngleVulkanImageRepresentation(SharedImageManager* manager,
                                     AngleVulkanImageBacking* backing,
                                     MemoryTypeTracker* tracker)
      : SkiaImageRepresentation(manager, backing, tracker) {}

  ~SkiaAngleVulkanImageRepresentation() override = default;

  // SkiaImageRepresentation implementation.
  std::vector<sk_sp<SkPromiseImageTexture>> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    if (!backing_impl()->BeginAccessSkia(/*readonly=*/true))
      return {};

    if (!backing_impl()->promise_texture_)
      return {};

    return {backing_impl()->promise_texture_};
  }

  void EndReadAccess() override { backing_impl()->EndAccessSkia(); }

  std::vector<sk_sp<SkPromiseImageTexture>> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    if (!backing_impl()->BeginAccessSkia(/*readonly=*/false))
      return {};

    if (!backing_impl()->promise_texture_)
      return {};

    return {backing_impl()->promise_texture_};
  }

  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    auto promise_textures =
        BeginWriteAccess(begin_semaphores, end_semaphores, end_state);
    if (promise_textures.empty())
      return {};

    auto surface = backing_impl()->context_state_->GetCachedSkSurface(
        backing_impl()->promise_texture_.get());

    // If surface properties are different from the last access, then we cannot
    // reuse the cached SkSurface.
    if (!surface || surface_props != surface->props() ||
        final_msaa_count != backing_impl()->surface_msaa_count_) {
      SkColorType sk_color_type =
          viz::ToClosestSkColorType(true /* gpu_compositing */, format());
      surface = SkSurface::MakeFromBackendTexture(
          backing_impl()->gr_context(), backing_impl()->backend_texture_,
          surface_origin(), final_msaa_count, sk_color_type,
          backing_impl()->color_space().ToSkColorSpace(), &surface_props);
      if (!surface) {
        backing_impl()->context_state_->EraseCachedSkSurface(
            backing_impl()->promise_texture_.get());
        return {};
      }
      backing_impl()->surface_msaa_count_ = final_msaa_count;
      backing_impl()->context_state_->CacheSkSurface(
          backing_impl()->promise_texture_.get(), surface);
    }

    [[maybe_unused]] int count = surface->getCanvas()->save();
    DCHECK_EQ(count, 1);

    write_surface_ = surface;
    return {surface};
  }

  void EndWriteAccess() override {
    if (write_surface_) {
      write_surface_->getCanvas()->restoreToCount(1);
      write_surface_.reset();
      DCHECK(backing_impl()->context_state_->CachedSkSurfaceIsUnique(
          backing_impl()->promise_texture_.get()));
    }
    backing_impl()->EndAccessSkia();
  }

 private:
  AngleVulkanImageBacking* backing_impl() const {
    return static_cast<AngleVulkanImageBacking*>(backing());
  }

  sk_sp<SkSurface> write_surface_;
};

AngleVulkanImageBacking::AngleVulkanImageBacking(
    const raw_ptr<SharedContextState>& context_state,
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage)
    : ClearTrackingSharedImageBacking(
          mailbox,
          format,
          size,
          color_space,
          surface_origin,
          alpha_type,
          usage,
          viz::ResourceSizes::UncheckedSizeInBytes<size_t>(size, format),
          false /* is_thread_safe */),
      context_state_(context_state) {}

AngleVulkanImageBacking::~AngleVulkanImageBacking() {
  DCHECK(!is_gl_write_in_process_);
  DCHECK(!is_skia_write_in_process_);
  DCHECK_EQ(gl_reads_in_process_, 0);
  DCHECK_EQ(skia_reads_in_process_, 0);

  if (promise_texture_) {
    context_state_->EraseCachedSkSurface(promise_texture_.get());
    promise_texture_.reset();
  }

  if (passthrough_texture_) {
    if (!gl::GLContext::GetCurrent())
      context_state_->MakeCurrent(/*surface=*/nullptr, /*needs_gl=*/true);

    if (!have_context())
      passthrough_texture_->MarkContextLost();

    passthrough_texture_.reset();
    egl_image_.reset();

    if (need_gl_finish_before_destroy_ && have_context()) {
      gl::GLApi* api = gl::g_current_gl_context;
      api->glFinishFn();
    }
  }

  if (vulkan_image_) {
    auto* fence_helper = context_state_->vk_context_provider()
                             ->GetDeviceQueue()
                             ->GetFenceHelper();
    fence_helper->EnqueueVulkanObjectCleanupForSubmittedWork(
        std::move(vulkan_image_));
  }
}

bool AngleVulkanImageBacking::Initialize(
    const base::span<const uint8_t>& data) {
  auto* device_queue = context_state_->vk_context_provider()->GetDeviceQueue();
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
    if (format().IsCompressed()) {
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

  GrVkImageInfo info = CreateGrVkImageInfo(vulkan_image_.get());
  backend_texture_ = GrBackendTexture(size().width(), size().height(), info);
  promise_texture_ = SkPromiseImageTexture::Make(backend_texture_);

  if (!data.empty()) {
    size_t stride = BitsPerPixel(format()) / 8 * size().width();
    WritePixels(data, stride);
    SetCleared();
  }

  return true;
}

bool AngleVulkanImageBacking::InitializeWihGMB(
    gfx::GpuMemoryBufferHandle handle) {
  auto* vulkan_implementation =
      context_state_->vk_context_provider()->GetVulkanImplementation();
  auto* device_queue = context_state_->vk_context_provider()->GetDeviceQueue();
  DCHECK(vulkan_implementation->CanImportGpuMemoryBuffer(device_queue,
                                                         handle.type));

  VkFormat vk_format = ToVkFormat(format().resource_format());
  auto vulkan_image = vulkan_implementation->CreateImageFromGpuMemoryHandle(
      device_queue, std::move(handle), size(), vk_format, color_space());

  if (!vulkan_image) {
    return false;
  }

  vulkan_image_ = std::move(vulkan_image);

  GrVkImageInfo info = CreateGrVkImageInfo(vulkan_image_.get());
  backend_texture_ = GrBackendTexture(size().width(), size().height(), info);
  promise_texture_ = SkPromiseImageTexture::Make(backend_texture_);

  SetCleared();

  return true;
}

SharedImageBackingType AngleVulkanImageBacking::GetType() const {
  return SharedImageBackingType::kAngleVulkan;
}

bool AngleVulkanImageBacking::UploadFromMemory(const SkPixmap& pixmap) {
  PrepareBackendTexture();
  DCHECK(backend_texture_.isValid());

  bool result = gr_context()->updateBackendTexture(backend_texture_, pixmap);
  DCHECK(result);
  SyncImageLayoutFromBackendTexture();
  return result;
}

void AngleVulkanImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  DCHECK(!in_fence);
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
AngleVulkanImageBacking::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  if (!passthrough_texture_ && !InitializePassthroughTexture())
    return nullptr;

  return std::make_unique<GLTexturePassthroughGLCommonRepresentation>(
      manager, this, this, tracker, passthrough_texture_);
}

std::unique_ptr<SkiaImageRepresentation> AngleVulkanImageBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  DCHECK_EQ(context_state_, context_state.get());
  return std::make_unique<SkiaAngleVulkanImageRepresentation>(manager, this,
                                                              tracker);
}

bool AngleVulkanImageBacking::GLTextureImageRepresentationBeginAccess(
    bool readonly) {
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

void AngleVulkanImageBacking::GLTextureImageRepresentationEndAccess(
    bool readonly) {
  if (readonly) {
    // For GL read access.
    if (gl_reads_in_process_ == 0) {
      LOG(DFATAL) << "The backing is not being read by GL";
      return;
    }

    --gl_reads_in_process_;

    // For the last GL read access, release texture from ANGLE.
    if (gl_reads_in_process_ == 0) {
      ReleaseTextureANGLE();
    }

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

void AngleVulkanImageBacking::GLTextureImageRepresentationRelease(
    bool have_context) {}

void AngleVulkanImageBacking::AcquireTextureANGLE() {
  gl::GLApi* api = gl::g_current_gl_context;
  GLuint texture = passthrough_texture_->service_id();
  // Acquire the texture, so ANGLE can access it.
  api->glAcquireTexturesANGLEFn(1, &texture, &layout_);
}

void AngleVulkanImageBacking::ReleaseTextureANGLE() {
  gl::GLApi* api = gl::g_current_gl_context;
  GLuint texture = passthrough_texture_->service_id();
  // Release the texture from ANGLE, so it can be used elsewhere.
  api->glReleaseTexturesANGLEFn(1, &texture, &layout_);
  // Releasing the texture will submit all related works to queue, so to be
  // safe, glFinish() should be called before releasing the VkImage.
  need_gl_finish_before_destroy_ = true;
}

void AngleVulkanImageBacking::PrepareBackendTexture() {
  auto vk_layout = GLImageLayoutToVkImageLayout(layout_);
  backend_texture_.setVkImageLayout(vk_layout);
}

void AngleVulkanImageBacking::SyncImageLayoutFromBackendTexture() {
  GrVkImageInfo info;
  bool result = backend_texture_.getVkImageInfo(&info);
  DCHECK(result);
  layout_ = VkImageLayoutToGLImageLayout(info.fImageLayout);
}

bool AngleVulkanImageBacking::BeginAccessSkia(bool readonly) {
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

void AngleVulkanImageBacking::EndAccessSkia() {
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

  // The backing is used by skia, so skia should submit related work to the
  // queue, and we can use vulkan fence helper to release the VkImage.
  // glFinish() is not necessary anymore.
  need_gl_finish_before_destroy_ = false;

  SyncImageLayoutFromBackendTexture();

  if (gl_reads_in_process_ > 0) {
    if (!gl::GLContext::GetCurrent())
      context_state_->MakeCurrent(/*surface=*/nullptr, /*needs_gl=*/true);
    // Recover GL access.
    AcquireTextureANGLE();
  }
}

bool AngleVulkanImageBacking::InitializePassthroughTexture() {
  DCHECK(vulkan_image_);
  DCHECK(!egl_image_);
  DCHECK(!passthrough_texture_);

  auto egl_image = base::MakeRefCounted<gl::GLImageEGLAngleVulkan>(size());
  if (!egl_image->Initialize(vulkan_image_->image(),
                             &vulkan_image_->create_info(),
                             GLInternalFormat(format()))) {
    return false;
  }

  scoped_refptr<gles2::TexturePassthrough> passthrough_texture;
  GLTextureImageBackingHelper::MakeTextureAndSetParameters(
      GL_TEXTURE_2D, /*service_id=*/0,
      /*framebuffer_attachment_angle=*/true, &passthrough_texture, nullptr);
  passthrough_texture->SetEstimatedSize(GetEstimatedSize());

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

void AngleVulkanImageBacking::WritePixels(
    const base::span<const uint8_t>& pixel_data,
    size_t stride) {
  SkPixmap pixmap(AsSkImageInfo(), pixel_data.data(), stride);
  UploadFromMemory(pixmap);
}

}  // namespace gpu
