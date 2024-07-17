// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/angle_vulkan_image_backing.h"

#include "base/logging.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_gl_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/skia_gl_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_util.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/gpu/MutableTextureState.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/ganesh/vk/GrVkBackendSurface.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/scoped_egl_image.h"
#include "ui/gl/scoped_restore_texture.h"

#define EGL_TEXTURE_INTERNAL_FORMAT_ANGLE 0x345D
#define EGL_VULKAN_IMAGE_ANGLE 0x34D3
#define EGL_VULKAN_IMAGE_CREATE_INFO_HI_ANGLE 0x34D4
#define EGL_VULKAN_IMAGE_CREATE_INFO_LO_ANGLE 0x34D5

namespace gpu {

namespace {

gl::ScopedEGLImage CreateEGLImage(VkImage image,
                                  const VkImageCreateInfo* create_info,
                                  unsigned int internal_format) {
  DCHECK(image != VK_NULL_HANDLE);
  DCHECK(create_info);

  uint64_t info = reinterpret_cast<uint64_t>(create_info);
  EGLint attribs[] = {
      EGL_VULKAN_IMAGE_CREATE_INFO_HI_ANGLE,
      static_cast<EGLint>((info >> 32) & 0xffffffff),
      EGL_VULKAN_IMAGE_CREATE_INFO_LO_ANGLE,
      static_cast<EGLint>(info & 0xffffffff),
      EGL_TEXTURE_INTERNAL_FORMAT_ANGLE,
      static_cast<EGLint>(internal_format),
      EGL_NONE,
  };

  return gl::MakeScopedEGLImage(EGL_NO_CONTEXT, EGL_VULKAN_IMAGE_ANGLE,
                                reinterpret_cast<EGLClientBuffer>(&image),
                                attribs);
}

}  // namespace

// GLTexturePassthroughImageRepresentation implementation.
class AngleVulkanImageBacking::
    GLTexturePassthroughAngleVulkanImageRepresentation
    : public GLTexturePassthroughImageRepresentation {
 public:
  GLTexturePassthroughAngleVulkanImageRepresentation(
      SharedImageManager* manager,
      AngleVulkanImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::vector<scoped_refptr<gles2::TexturePassthrough>> textures)
      : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
        textures_(std::move(textures)) {
    DCHECK_EQ(textures_.size(), NumPlanesExpected());
  }

  ~GLTexturePassthroughAngleVulkanImageRepresentation() override = default;

 private:
  AngleVulkanImageBacking* backing_impl() {
    return static_cast<AngleVulkanImageBacking*>(backing());
  }

  // GLTexturePassthroughImageRepresentation:
  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough(
      int plane_index) override {
    DCHECK(format().IsValidPlaneIndex(plane_index));
    return textures_[plane_index];
  }
  bool BeginAccess(GLenum mode) override {
    DCHECK(mode_ == 0);
    mode_ = mode;
    return backing_impl()->BeginAccessGLTexturePassthrough(mode_);
  }
  void EndAccess() override {
    DCHECK(mode_ != 0);
    backing_impl()->EndAccessGLTexturePassthrough(mode_);
    mode_ = 0;
  }

  std::vector<scoped_refptr<gles2::TexturePassthrough>> textures_;
  GLenum mode_ = 0;
};

class AngleVulkanImageBacking::SkiaAngleVulkanImageRepresentation
    : public SkiaGaneshImageRepresentation {
 public:
  SkiaAngleVulkanImageRepresentation(GrDirectContext* gr_context,
                                     SharedImageManager* manager,
                                     AngleVulkanImageBacking* backing,
                                     MemoryTypeTracker* tracker)
      : SkiaGaneshImageRepresentation(gr_context, manager, backing, tracker),
        context_state_(backing_impl()->context_state_) {}

  ~SkiaAngleVulkanImageRepresentation() override = default;

  // SkiaImageRepresentation implementation.
  std::vector<sk_sp<GrPromiseImageTexture>> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override {
    if (!backing_impl()->BeginAccessSkia(/*readonly=*/true)) {
      return {};
    }

    return backing_impl()->GetPromiseTextures();
  }

  void EndReadAccess() override { backing_impl()->EndAccessSkia(); }

  std::vector<sk_sp<GrPromiseImageTexture>> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override {
    if (!backing_impl()->BeginAccessSkia(/*readonly=*/false)) {
      return {};
    }

    return backing_impl()->GetPromiseTextures();
  }

  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override {
    auto promise_textures =
        BeginWriteAccess(begin_semaphores, end_semaphores, end_state);
    if (promise_textures.empty()) {
      return {};
    }

    std::vector<sk_sp<SkSurface>> surfaces;
    surfaces.reserve(promise_textures.size());
    for (size_t plane = 0; plane < promise_textures.size(); ++plane) {
      auto promise_texture = promise_textures[plane];

      auto surface = context_state_->GetCachedSkSurface(promise_texture.get());

      // If surface properties are different from the last access, then we
      // cannot reuse the cached SkSurface.
      if (!surface || surface_props != surface->props() ||
          final_msaa_count != backing_impl()->surface_msaa_count_) {
        SkColorType sk_color_type = viz::ToClosestSkColorType(
            /*gpu_compositing=*/true, format(), plane);
        surface = SkSurfaces::WrapBackendTexture(
            backing_impl()->gr_context(), promise_texture->backendTexture(),
            surface_origin(), final_msaa_count, sk_color_type,
            backing_impl()->color_space().ToSkColorSpace(), &surface_props);
        if (!surface) {
          context_state_->EraseCachedSkSurface(promise_texture.get());
          return {};
        }
        context_state_->CacheSkSurface(promise_texture.get(), surface);
      }

      [[maybe_unused]] int count = surface->getCanvas()->save();
      DCHECK_EQ(count, 1);

      surfaces.push_back(std::move(surface));
    }

    backing_impl()->surface_msaa_count_ = final_msaa_count;
    write_surfaces_ = surfaces;
    return surfaces;
  }

  void EndWriteAccess() override {
    for (auto& write_surface : write_surfaces_) {
      write_surface->getCanvas()->restoreToCount(1);
    }
    write_surfaces_.clear();

#if DCHECK_IS_ON()
    for (auto& promise_texture : backing_impl()->GetPromiseTextures()) {
      DCHECK(context_state_->CachedSkSurfaceIsUnique(promise_texture.get()));
    }
#endif

    backing_impl()->EndAccessSkia();
  }

 private:
  AngleVulkanImageBacking* backing_impl() const {
    return static_cast<AngleVulkanImageBacking*>(backing());
  }

  const scoped_refptr<SharedContextState> context_state_;
  std::vector<sk_sp<SkSurface>> write_surfaces_;
};

AngleVulkanImageBacking::AngleVulkanImageBacking(
    scoped_refptr<SharedContextState> context_state,
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    gpu::SharedImageUsageSet usage,
    std::string debug_label)
    : ClearTrackingSharedImageBacking(mailbox,
                                      format,
                                      size,
                                      color_space,
                                      surface_origin,
                                      alpha_type,
                                      usage,
                                      std::move(debug_label),
                                      format.EstimatedSizeInBytes(size),
                                      /*is_thread_safe=*/false),
      context_state_(std::move(context_state)) {}

AngleVulkanImageBacking::~AngleVulkanImageBacking() {
  DCHECK(!is_gl_write_in_process_);
  DCHECK(!is_skia_write_in_process_);
  DCHECK_EQ(gl_reads_in_process_, 0);
  DCHECK_EQ(skia_reads_in_process_, 0);

  auto* fence_helper =
      context_state_->vk_context_provider()->GetDeviceQueue()->GetFenceHelper();

  for (auto& vk_texture : vk_textures_) {
    if (vk_texture.promise_texture) {
      context_state_->EraseCachedSkSurface(vk_texture.promise_texture.get());
    }

    if (vk_texture.vulkan_image) {
      fence_helper->EnqueueVulkanObjectCleanupForSubmittedWork(
          std::move(vk_texture.vulkan_image));
    }
  }
  vk_textures_.clear();

  if (!gl_textures_.empty()) {
    if (!gl::GLContext::GetCurrent()) {
      context_state_->MakeCurrent(/*surface=*/nullptr, /*needs_gl=*/true);
    }

    if (!have_context()) {
      for (auto& gl_texture : gl_textures_) {
        gl_texture.passthrough_texture->MarkContextLost();
      }
    }
    gl_textures_.clear();

    if (need_gl_finish_before_destroy_ && have_context()) {
      gl::GLApi* api = gl::g_current_gl_context;
      api->glFinishFn();
    }
  }
}

bool AngleVulkanImageBacking::Initialize(
    const base::span<const uint8_t>& data) {
  auto* device_queue = context_state_->vk_context_provider()->GetDeviceQueue();

  constexpr gpu::SharedImageUsageSet usages_needing_color_attachment =
      SHARED_IMAGE_USAGE_GLES2_WRITE | SHARED_IMAGE_USAGE_RASTER_WRITE |
      SHARED_IMAGE_USAGE_DISPLAY_WRITE;

  VkImageUsageFlags vk_usage = VK_IMAGE_USAGE_SAMPLED_BIT |
                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  if (usage().HasAny(usages_needing_color_attachment)) {
    vk_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    if (format().IsCompressed()) {
      DLOG(ERROR) << "ETC1 format cannot be used as color attachment.";
      return false;
    }
  }

  // External sampling is supported only when initializing from GMB.
  CHECK(!format().PrefersExternalSampler());
  int num_planes = format().NumberOfPlanes();
  vk_textures_.reserve(num_planes);
  for (int plane = 0; plane < num_planes; ++plane) {
    VkFormat vk_format = ToVkFormat(format(), plane);
    gfx::Size plane_size = format().GetPlaneSize(plane, size());
    VkImageCreateFlags vk_create_flags = 0;
    auto vulkan_image =
        VulkanImage::Create(device_queue, plane_size, vk_format, vk_usage,
                            vk_create_flags, VK_IMAGE_TILING_OPTIMAL);

    if (!vulkan_image) {
      return false;
    }

    vk_textures_.emplace_back(std::move(vulkan_image), format(), color_space());
  }

  if (!data.empty()) {
    DCHECK(format().is_single_plane());
    auto image_info = AsSkImageInfo();
    if (data.size() != image_info.computeMinByteSize()) {
      DLOG(ERROR) << "Invalid initial pixel data size";
      return false;
    }
    SkPixmap pixmap(image_info, data.data(), image_info.minRowBytes());
    UploadFromMemory({pixmap});
    SetCleared();
  }

  return true;
}

bool AngleVulkanImageBacking::InitializeWihGMB(
    gfx::GpuMemoryBufferHandle handle) {
  DCHECK(format().is_single_plane() || format().PrefersExternalSampler());

  auto* vulkan_implementation =
      context_state_->vk_context_provider()->GetVulkanImplementation();
  auto* device_queue = context_state_->vk_context_provider()->GetDeviceQueue();
  DCHECK(vulkan_implementation->CanImportGpuMemoryBuffer(device_queue,
                                                         handle.type));

  VkFormat vk_format = format().PrefersExternalSampler()
                           ? ToVkFormatExternalSampler(format())
                           : ToVkFormatSinglePlanar(format());
  auto vulkan_image = vulkan_implementation->CreateImageFromGpuMemoryHandle(
      device_queue, std::move(handle), size(), vk_format, color_space());

  if (!vulkan_image) {
    return false;
  }

  vk_textures_.emplace_back(std::move(vulkan_image), format(), color_space());

  SetCleared();

  return true;
}

SharedImageBackingType AngleVulkanImageBacking::GetType() const {
  return SharedImageBackingType::kAngleVulkan;
}

bool AngleVulkanImageBacking::UploadFromMemory(
    const std::vector<SkPixmap>& pixmaps) {
  DCHECK_EQ(vk_textures_.size(), pixmaps.size());

  PrepareBackendTexture();

  bool updated = true;
  for (size_t i = 0; i < vk_textures_.size(); ++i) {
    DCHECK(vk_textures_[i].backend_texture.isValid());
    bool result = gr_context()->updateBackendTexture(
        vk_textures_[i].backend_texture, pixmaps[i], surface_origin());
    updated = updated && result;
  }

  SyncImageLayoutFromBackendTexture();
  return updated;
}

void AngleVulkanImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  DCHECK(!in_fence);
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
AngleVulkanImageBacking::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  if (gl_textures_.empty() && !InitializePassthroughTexture()) {
    return nullptr;
  }

  std::vector<scoped_refptr<gles2::TexturePassthrough>> textures;
  textures.reserve(gl_textures_.size());
  for (auto& gl_texture : gl_textures_) {
    textures.push_back(gl_texture.passthrough_texture);
  }

  return std::make_unique<GLTexturePassthroughAngleVulkanImageRepresentation>(
      manager, this, tracker, std::move(textures));
}

std::unique_ptr<SkiaGaneshImageRepresentation>
AngleVulkanImageBacking::ProduceSkiaGanesh(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  if (context_state->GrContextIsVulkan()) {
    DCHECK_EQ(context_state_, context_state);
    return std::make_unique<SkiaAngleVulkanImageRepresentation>(
        context_state->gr_context(), manager, this, tracker);
  }
  // If it is not vulkan context, it must be GL context being used with Skia
  // over passthrough command decoder.
  DCHECK(context_state->GrContextIsGL());
  auto gl_representation = ProduceGLTexturePassthrough(manager, tracker);
  if (!gl_representation) {
    return nullptr;
  }
  return SkiaGLImageRepresentation::Create(std::move(gl_representation),
                                           std::move(context_state), manager,
                                           this, tracker);
}

bool AngleVulkanImageBacking::BeginAccessGLTexturePassthrough(GLenum mode) {
  bool readonly = mode != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
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

void AngleVulkanImageBacking::EndAccessGLTexturePassthrough(GLenum mode) {
  bool readonly = mode != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
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

std::vector<sk_sp<GrPromiseImageTexture>>
AngleVulkanImageBacking::GetPromiseTextures() {
  std::vector<sk_sp<GrPromiseImageTexture>> promise_textures;
  promise_textures.reserve(vk_textures_.size());
  for (auto& vk_texture : vk_textures_) {
    DCHECK(vk_texture.promise_texture);
    promise_textures.push_back(vk_texture.promise_texture);
  }
  return promise_textures;
}

void AngleVulkanImageBacking::AcquireTextureANGLE() {
  DCHECK(!gl_textures_.empty());
  DCHECK_GE(kMaxTextures, gl_textures_.size());

  gl::GLApi* api = gl::g_current_gl_context;
  // Acquire the texture(s), so ANGLE can access it.
  api->glAcquireTexturesANGLEFn(gl_textures_.size(), gl_texture_ids_.data(),
                                gl_layouts_.data());
}

void AngleVulkanImageBacking::ReleaseTextureANGLE() {
  DCHECK(!gl_textures_.empty());
  DCHECK_GE(kMaxTextures, gl_textures_.size());

  gl::GLApi* api = gl::g_current_gl_context;
  // Release the texture(s) from ANGLE, so it can be used elsewhere.
  api->glReleaseTexturesANGLEFn(gl_textures_.size(), gl_texture_ids_.data(),
                                gl_layouts_.data());
  // Releasing the texture will submit all related works to queue, so to be
  // safe, glFinish() should be called before releasing the VkImage.
  need_gl_finish_before_destroy_ = true;
}

void AngleVulkanImageBacking::PrepareBackendTexture() {
  DCHECK_GE(kMaxTextures, vk_textures_.size());

  for (size_t i = 0; i < vk_textures_.size(); ++i) {
    auto vk_layout = GLImageLayoutToVkImageLayout(gl_layouts_[i]);
    GrBackendTextures::SetVkImageLayout(&vk_textures_[i].backend_texture,
                                        vk_layout);
  }
}

void AngleVulkanImageBacking::SyncImageLayoutFromBackendTexture() {
  DCHECK_GE(kMaxTextures, vk_textures_.size());

  for (size_t i = 0; i < vk_textures_.size(); ++i) {
    GrVkImageInfo info = vk_textures_[i].GetGrVkImageInfo();
    gl_layouts_[i] = VkImageLayoutToGLImageLayout(info.fImageLayout);
  }
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
  DCHECK(gl_textures_.empty());

  // It is not possible to import into GL when using external sampling.
  // Short-circuit out if this is requested (it should not be requested in
  // production, but might be in fuzzing flows).
  if (format().PrefersExternalSampler()) {
    LOG(ERROR)
        << "Importing textures with external sampling into GL is not possible";
    return false;
  }

  int num_planes = format().NumberOfPlanes();
  gl_textures_.reserve(num_planes);
  for (int plane = 0; plane < num_planes; ++plane) {
    auto& vulkan_image = vk_textures_[plane].vulkan_image;
    DCHECK(vulkan_image);

    auto format_desc =
        context_state_->GetGLFormatCaps().ToGLFormatDesc(format(), plane);
    auto egl_image =
        CreateEGLImage(vulkan_image->image(), &vulkan_image->create_info(),
                       format_desc.image_internal_format);
    if (!egl_image.is_valid()) {
      LOG(ERROR) << "Error creating EGLImage: " << ui::GetLastEGLErrorString();
      gl_textures_.clear();
      gl_texture_ids_.fill(0u);
      return false;
    }

    scoped_refptr<gles2::TexturePassthrough> passthrough_texture;
    GLuint texture_id = MakeTextureAndSetParameters(
        GL_TEXTURE_2D,
        /*framebuffer_attachment_angle=*/true, &passthrough_texture, nullptr);
    passthrough_texture->SetEstimatedSize(GetEstimatedSize());

    gl::GLApi* api = gl::g_current_gl_context;
    gl::ScopedRestoreTexture scoped_restore(api, GL_TEXTURE_2D);
    api->glBindTextureFn(GL_TEXTURE_2D, texture_id);

    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, egl_image.get());

    if (gl::g_current_gl_driver->ext.b_GL_KHR_debug) {
      const std::string label =
          "SharedImage_AngleVulkan" + CreateLabelForSharedImageUsage(usage());
      api->glObjectLabelFn(GL_TEXTURE, texture_id, label.size(), label.c_str());
    }

    auto& gl_texture = gl_textures_.emplace_back();
    gl_texture.egl_image = std::move(egl_image);
    gl_texture.passthrough_texture = std::move(passthrough_texture);

    gl_texture_ids_[plane] = texture_id;
  }

  return true;
}

AngleVulkanImageBacking::TextureHolderGL::TextureHolderGL() = default;
AngleVulkanImageBacking::TextureHolderGL::TextureHolderGL(
    TextureHolderGL&& other) = default;
AngleVulkanImageBacking::TextureHolderGL&
AngleVulkanImageBacking::TextureHolderGL::operator=(TextureHolderGL&& other) =
    default;
AngleVulkanImageBacking::TextureHolderGL::~TextureHolderGL() = default;

}  // namespace gpu
