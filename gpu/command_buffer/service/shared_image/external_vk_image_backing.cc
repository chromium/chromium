// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/shared_image/external_vk_image_backing.h"

#include <utility>
#include <vector>

#include "base/bits.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/shared_image/external_vk_image_gl_representation.h"
#include "gpu/command_buffer/service/shared_image/external_vk_image_overlay_representation.h"
#include "gpu/command_buffer/service/shared_image/external_vk_image_skia_representation.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_holder.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_gl_utils.h"
#include "gpu/command_buffer/service/shared_image/skia_gl_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "gpu/vulkan/vma_wrapper.h"
#include "gpu/vulkan/vulkan_command_buffer.h"
#include "gpu/vulkan/vulkan_command_pool.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_util.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/gpu/MutableTextureState.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "third_party/skia/include/gpu/ganesh/vk/GrVkBackendSemaphore.h"
#include "third_party/skia/include/gpu/ganesh/vk/GrVkBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/vk/GrVkTypes.h"
#include "third_party/skia/include/gpu/vk/VulkanMutableTextureState.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/scoped_binders.h"

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_DAWN)
#include "gpu/command_buffer/service/shared_image/external_vk_image_dawn_representation.h"
#if BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
#include "gpu/command_buffer/service/shared_image/dawn_gl_texture_representation.h"
#endif
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include "gpu/vulkan/fuchsia/vulkan_fuchsia_ext.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

#define GL_DEDICATED_MEMORY_OBJECT_EXT 0x9581
#define GL_TEXTURE_TILING_EXT 0x9580
#define GL_TILING_TYPES_EXT 0x9583
#define GL_OPTIMAL_TILING_EXT 0x9584
#define GL_LINEAR_TILING_EXT 0x9585
#define GL_HANDLE_TYPE_OPAQUE_FD_EXT 0x9586
#define GL_HANDLE_TYPE_OPAQUE_WIN32_EXT 0x9587
#define GL_HANDLE_TYPE_ZIRCON_VMO_ANGLE 0x93AE
#define GL_HANDLE_TYPE_ZIRCON_EVENT_ANGLE 0x93AF

namespace gpu {

namespace {

class ScopedDedicatedMemoryObject {
 public:
  explicit ScopedDedicatedMemoryObject(gl::GLApi* api) : api_(api) {
    api_->glCreateMemoryObjectsEXTFn(1, &id_);
    int dedicated = GL_TRUE;
    api_->glMemoryObjectParameterivEXTFn(id_, GL_DEDICATED_MEMORY_OBJECT_EXT,
                                         &dedicated);
  }
  ~ScopedDedicatedMemoryObject() { api_->glDeleteMemoryObjectsEXTFn(1, &id_); }

  GLuint id() const { return id_; }

 private:
  raw_ptr<gl::GLApi> const api_;
  GLuint id_;
};

bool UseSeparateGLTexture(SharedContextState* context_state,
                          viz::SharedImageFormat format) {
  if (!context_state->support_vulkan_external_object())
    return true;

  if (format != viz::SinglePlaneFormat::kBGRA_8888) {
    return false;
  }

  auto* gl_context = context_state->real_context();
  const auto* version_info = gl_context->GetVersionInfo();
  const auto& ext = gl_context->GetCurrentGL()->Driver->ext;
  if (!ext.b_GL_EXT_texture_format_BGRA8888)
    return true;

  if (!version_info->is_angle)
    return false;

  // If ANGLE is using vulkan, there is no problem for importing BGRA8888
  // textures.
  if (version_info->is_angle_vulkan)
    return false;

  // ANGLE claims GL_EXT_texture_format_BGRA8888, but glTexStorageMem2DEXT
  // doesn't work correctly.
  // TODO(crbug.com/angleproject/4831): fix ANGLE and return false.
  return true;
}

bool UseMinimalUsageFlags(SharedContextState* context_state) {
  return context_state->support_gl_external_object_flags();
}

void WaitSemaphoresOnGrContext(GrDirectContext* gr_context,
                               std::vector<ExternalSemaphore>* semaphores) {
  DCHECK(!gr_context->abandoned());
  std::vector<GrBackendSemaphore> backend_semaphores;
  backend_semaphores.reserve(semaphores->size());
  for (auto& semaphore : *semaphores) {
    backend_semaphores.emplace_back(
        GrBackendSemaphores::MakeVk(semaphore.GetVkSemaphore()));
  }
  gr_context->wait(backend_semaphores.size(), backend_semaphores.data(),
                   /*deleteSemaphoresAfterWait=*/false);
}

}  // namespace

// static
std::unique_ptr<ExternalVkImageBacking> ExternalVkImageBacking::Create(
    scoped_refptr<SharedContextState> context_state,
    VulkanCommandPool* command_pool,
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    const base::flat_map<VkFormat, VkImageUsageFlags>& image_usage_cache,
    base::span<const uint8_t> pixel_data) {
  bool is_external = context_state->support_vulkan_external_object();

  auto* device_queue = context_state->vk_context_provider()->GetDeviceQueue();

  SharedImageUsageSet usages_needing_color_attachment;

  usages_needing_color_attachment =
      SHARED_IMAGE_USAGE_GLES2_WRITE | SHARED_IMAGE_USAGE_RASTER_WRITE |
      SHARED_IMAGE_USAGE_DISPLAY_WRITE | SHARED_IMAGE_USAGE_WEBGPU_WRITE;

  VkImageUsageFlags vk_usage = VK_IMAGE_USAGE_SAMPLED_BIT |
                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  if (usage.HasAny(usages_needing_color_attachment)) {
    vk_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    if (format.IsCompressed()) {
      DLOG(ERROR) << "ETC1 format cannot be used as color attachment.";
      return nullptr;
    }
  }

  // Must request all available image usage flags if aliasing GL texture. This
  // is a spec requirement per EXT_memory_object. However, if
  // ANGLE_memory_object_flags is supported, usage flags can be arbitrary.
  bool request_all_flags = is_external && (HasGLES2ReadOrWriteUsage(usage)) &&
                           !UseMinimalUsageFlags(context_state.get());

  VkImageCreateFlags vk_create = 0;

  // Using external sampling is only possible when creating from GMBs.
  CHECK(!format.PrefersExternalSampler());
  int num_planes = format.NumberOfPlanes();
  std::vector<TextureHolderVk> textures;
  textures.reserve(num_planes);

  size_t estimated_size = 0;
  for (int plane = 0; plane < format.NumberOfPlanes(); ++plane) {
    gfx::Size plane_size = format.GetPlaneSize(plane, size);
    VkFormat vk_format = ToVkFormat(format, plane);

    auto it = image_usage_cache.find(vk_format);
    CHECK(it != image_usage_cache.end(), base::NotFatalUntil::M130);
    auto vk_tiling_usage = it->second;

    // Requested usage flags must be supported.
    DCHECK_EQ(vk_usage & vk_tiling_usage, vk_usage);

    VkImageUsageFlags vk_plane_usage = vk_usage;
    if (request_all_flags) {
      vk_plane_usage |= vk_tiling_usage;
    }

    std::unique_ptr<VulkanImage> image;
    if (is_external) {
      image = VulkanImage::CreateWithExternalMemory(
          device_queue, plane_size, vk_format, vk_plane_usage, vk_create,
          VK_IMAGE_TILING_OPTIMAL);
    } else {
      image = VulkanImage::Create(device_queue, plane_size, vk_format,
                                  vk_plane_usage, vk_create,
                                  VK_IMAGE_TILING_OPTIMAL);
    }
    if (!image) {
      return nullptr;
    }

    estimated_size += image->device_size();
    textures.emplace_back(std::move(image), format, color_space);
  }

  bool use_separate_gl_texture =
      UseSeparateGLTexture(context_state.get(), format);
  auto backing = std::make_unique<ExternalVkImageBacking>(
      base::PassKey<ExternalVkImageBacking>(), mailbox, format, size,
      color_space, surface_origin, alpha_type, usage, std::move(debug_label),
      estimated_size, std::move(context_state), std::move(textures),
      command_pool, use_separate_gl_texture);

  if (!pixel_data.empty()) {
    auto image_info = backing->AsSkImageInfo();
    if (pixel_data.size() != image_info.computeMinByteSize()) {
      DLOG(ERROR) << "Invalid initial pixel data size";
      return nullptr;
    }
    SkPixmap pixmap(image_info, pixel_data.data(), image_info.minRowBytes());
    backing->UploadToVkImage({pixmap});

    // Mark the backing as cleared.
    backing->SetCleared();
    backing->latest_content_ = kInVkImage;
  }

  return backing;
}

// static
std::unique_ptr<ExternalVkImageBacking> ExternalVkImageBacking::CreateFromGMB(
    scoped_refptr<SharedContextState> context_state,
    VulkanCommandPool* command_pool,
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle handle,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    std::optional<gfx::BufferUsage> buffer_usage) {
  if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(size,
                                                     ToBufferFormat(format))) {
    DLOG(ERROR) << "Invalid image size for format.";
    return nullptr;
  }

  auto* vulkan_implementation =
      context_state->vk_context_provider()->GetVulkanImplementation();
  auto* device_queue = context_state->vk_context_provider()->GetDeviceQueue();
  DCHECK(vulkan_implementation->CanImportGpuMemoryBuffer(device_queue,
                                                         handle.type));

  // Note that `format` is implicitly assumed to not be using per-plane
  // sampling as we create only one TextureHolderVk below.
  VkFormat vk_format = format.PrefersExternalSampler()
                           ? ToVkFormatExternalSampler(format)
                           : ToVkFormatSinglePlanar(format);
  auto image = vulkan_implementation->CreateImageFromGpuMemoryHandle(
      device_queue, handle.Clone(), size, vk_format, color_space);
  if (!image) {
    DLOG(ERROR) << "Failed to create VkImage from GpuMemoryHandle.";
    return nullptr;
  }

  size_t estimated_size = image->device_size();

  std::vector<TextureHolderVk> textures;
  textures.reserve(1);
  textures.emplace_back(std::move(image), format, color_space);

  bool use_separate_gl_texture =
      UseSeparateGLTexture(context_state.get(), format);
  auto backing = std::make_unique<ExternalVkImageBacking>(
      base::PassKey<ExternalVkImageBacking>(), mailbox, format, size,
      color_space, surface_origin, alpha_type, usage, std::move(debug_label),
      estimated_size, std::move(context_state), std::move(textures),
      command_pool, use_separate_gl_texture, std::move(handle),
      std::move(buffer_usage));
  backing->SetCleared();
  return backing;
}

std::unique_ptr<ExternalVkImageBacking>
ExternalVkImageBacking::CreateWithPixmap(
    scoped_refptr<SharedContextState> context_state,
    VulkanCommandPool* command_pool,
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    gfx::BufferUsage buffer_usage) {
#if BUILDFLAG(IS_OZONE)
  // Create a pixmap.
  gfx::BufferFormat buffer_format = ToBufferFormat(format);
  VulkanDeviceQueue* device_queue = nullptr;
  if (context_state->vk_context_provider()) {
    device_queue = context_state->vk_context_provider()->GetDeviceQueue();
  }
  scoped_refptr<gfx::NativePixmap> pixmap =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateNativePixmap(surface_handle, device_queue, size,
                               buffer_format, buffer_usage);
  if (!pixmap) {
    DLOG(ERROR) << "Failed to create native pixmap";
    return nullptr;
  }

  // Create a handle from pixmap.
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::GpuMemoryBufferType::NATIVE_PIXMAP;
  handle.native_pixmap_handle = pixmap->ExportHandle();

  // Create backing from the handle.
  return CreateFromGMB(std::move(context_state), command_pool, mailbox,
                       std::move(handle), format, size, color_space,
                       surface_origin, alpha_type, usage,
                       std::move(debug_label));
#else
  return nullptr;
#endif  // BUILDFLAG(IS_OZONE)
}

ExternalVkImageBacking::ExternalVkImageBacking(
    base::PassKey<ExternalVkImageBacking>,
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    size_t estimated_size_bytes,
    scoped_refptr<SharedContextState> context_state,
    std::vector<TextureHolderVk> vk_textures,
    VulkanCommandPool* command_pool,
    bool use_separate_gl_texture,
    gfx::GpuMemoryBufferHandle handle,
    std::optional<gfx::BufferUsage> buffer_usage)
    : ClearTrackingSharedImageBacking(mailbox,
                                      format,
                                      size,
                                      color_space,
                                      surface_origin,
                                      alpha_type,
                                      usage,
                                      std::move(debug_label),
                                      estimated_size_bytes,
                                      /*is_thread_safe=*/false,
                                      std::move(buffer_usage)),
      context_state_(std::move(context_state)),
      vk_textures_(std::move(vk_textures)),
      command_pool_(command_pool),
      use_separate_gl_texture_(use_separate_gl_texture) {
#if BUILDFLAG(IS_OZONE)
  if (!handle.is_null()) {
    // Create a pixmap is there is a valid handle.
    pixmap_ = ui::OzonePlatform::GetInstance()
                  ->GetSurfaceFactoryOzone()
                  ->CreateNativePixmapFromHandle(
                      kNullSurfaceHandle, size, ToBufferFormat(format),
                      std::move(handle.native_pixmap_handle));
  }
#endif  // BUILDFLAG(IS_OZONE)
}

ExternalVkImageBacking::~ExternalVkImageBacking() {
  auto semaphores = std::move(read_semaphores_);
  if (write_semaphore_) {
    semaphores.emplace_back(std::move(write_semaphore_));
  }

  if (!semaphores.empty() && !context_state()->gr_context()->abandoned()) {
    WaitSemaphoresOnGrContext(context_state()->gr_context(), &semaphores);
    ReturnPendingSemaphoresWithFenceHelper(std::move(semaphores));
  }

  for (auto& vk_texture : vk_textures_) {
    fence_helper()->EnqueueVulkanObjectCleanupForSubmittedWork(
        std::move(vk_texture.vulkan_image));
  }
  vk_textures_.clear();

  if (!gl_textures_.empty()) {
    // Ensure that a context is current before glDeleteTexture().
    MakeGLContextCurrent();
    if (!have_context()) {
      for (auto& gl_texture : gl_textures_) {
        gl_texture.SetContextLost();
      }
    }
    gl_textures_.clear();
  }
}

std::vector<GLenum> ExternalVkImageBacking::GetVkImageLayoutsForGL() {
  std::vector<GLenum> layouts;
  layouts.reserve(vk_textures_.size());
  for (auto& vk_texture : vk_textures_) {
    GrVkImageInfo info = vk_texture.GetGrVkImageInfo();
    DCHECK_EQ(info.fCurrentQueueFamily, VK_QUEUE_FAMILY_EXTERNAL);
    DCHECK_NE(info.fImageLayout, VK_IMAGE_LAYOUT_UNDEFINED);
    DCHECK_NE(info.fImageLayout, VK_IMAGE_LAYOUT_PREINITIALIZED);
    layouts.push_back(VkImageLayoutToGLImageLayout(info.fImageLayout));
  }
  return layouts;
}

std::vector<sk_sp<GrPromiseImageTexture>>
ExternalVkImageBacking::GetPromiseTextures() {
  std::vector<sk_sp<GrPromiseImageTexture>> promise_textures;
  promise_textures.reserve(vk_textures_.size());
  for (auto& vk_texture : vk_textures_) {
    promise_textures.push_back(vk_texture.promise_texture);
  }
  return promise_textures;
}

bool ExternalVkImageBacking::BeginAccess(
    bool readonly,
    std::vector<ExternalSemaphore>* external_semaphores,
    bool is_gl) {
  DLOG_IF(ERROR, gl_reads_in_progress_ != 0 && !is_gl)
      << "Backing is being accessed by both GL and Vulkan.";
  // Do not need do anything for the second and following GL read access.
  if (is_gl && readonly && gl_reads_in_progress_) {
    ++gl_reads_in_progress_;
    return true;
  }

  if (readonly && !reads_in_progress_) {
    UpdateContent(kInVkImage);
    if (!gl_textures_.empty()) {
      UpdateContent(kInGLTexture);
    }
  }

  if (gl_reads_in_progress_ && need_synchronization()) {
    // To avoid concurrent read access from both GL and vulkan, if there is
    // unfinished GL read access, we will release the GL texture temporarily.
    // And when this vulkan access is over, we will acquire the GL texture to
    // resume the GL access.
    DCHECK(!is_gl);
    DCHECK(readonly);
    DCHECK_EQ(vk_textures_.size(), gl_textures_.size());

    std::vector<GLuint> texture_ids;
    for (auto& gl_texture : gl_textures_) {
      texture_ids.push_back(gl_texture.GetServiceId());
    }

    MakeGLContextCurrent();

    auto release_semaphore =
        ExternalVkImageGLRepresentationShared::ReleaseTexture(
            external_semaphore_pool(), texture_ids, GetVkImageLayoutsForGL());
    if (!release_semaphore) {
      context_state_->MarkContextLost();
      return false;
    }
    EndAccessInternal(readonly, std::move(release_semaphore));
  }

  if (!BeginAccessInternal(readonly, external_semaphores))
    return false;

  if (!is_gl)
    return true;

  if (need_synchronization() && external_semaphores->empty()) {
    // For the first time GL BeginAccess(), external_semaphores could be empty,
    // since the Vulkan usage will not provide semaphore for EndAccess() call,
    // if ProduceGL*() is never called. In this case, image layout and queue
    // family will not be ready for GL access as well.
    auto* gr_context = context_state()->gr_context();
    for (auto& vk_texture : vk_textures_) {
      gr_context->setBackendTextureState(
          vk_texture.backend_texture,
          skgpu::MutableTextureStates::MakeVulkan(
              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
              VK_QUEUE_FAMILY_EXTERNAL));
    }

    ExternalSemaphore external_semaphore =
        external_semaphore_pool()->GetOrCreateSemaphore();
    GrBackendSemaphore semaphore =
        GrBackendSemaphores::MakeVk(external_semaphore.GetVkSemaphore());

    GrFlushInfo flush_info;
    flush_info.fNumSemaphores = 1;
    flush_info.fSignalSemaphores = &semaphore;

    if (gr_context->flush(flush_info) != GrSemaphoresSubmitted::kYes) {
      LOG(ERROR) << "Failed to create a signaled semaphore";
      return false;
    }

    if (!gr_context->submit()) {
      LOG(ERROR) << "Failed GrContext submit";
      return false;
    }

    external_semaphores->push_back(std::move(external_semaphore));
  }

  if (readonly) {
    DCHECK(!gl_reads_in_progress_);
    gl_reads_in_progress_ = 1;
  }
  return true;
}

void ExternalVkImageBacking::EndAccess(bool readonly,
                                       ExternalSemaphore external_semaphore,
                                       bool is_gl) {
  if (is_gl && readonly) {
    DCHECK(gl_reads_in_progress_);
    if (--gl_reads_in_progress_ > 0) {
      DCHECK(!external_semaphore);
      return;
    }
  }

  EndAccessInternal(readonly, std::move(external_semaphore));
  if (!readonly) {
    if (use_separate_gl_texture()) {
      latest_content_ = is_gl ? kInGLTexture : kInVkImage;
    } else {
      latest_content_ = kInVkImage;
    }
  }

  if (gl_reads_in_progress_ && need_synchronization()) {
    // When vulkan read access is finished, if there is unfinished GL read
    // access, we need to resume GL read access.
    DCHECK(!is_gl);
    DCHECK(readonly);
    DCHECK_EQ(vk_textures_.size(), gl_textures_.size());

    std::vector<GLuint> texture_ids;
    for (auto& gl_texture : gl_textures_) {
      texture_ids.push_back(gl_texture.GetServiceId());
    }

    MakeGLContextCurrent();
    std::vector<ExternalSemaphore> external_semaphores;
    BeginAccessInternal(true, &external_semaphores);
    DCHECK_LE(external_semaphores.size(), 1u);

    for (auto& semaphore : external_semaphores) {
      ExternalVkImageGLRepresentationShared::AcquireTexture(
          &semaphore, texture_ids, GetVkImageLayoutsForGL());
    }
    // |external_semaphores| has been waited on a GL context, so it can not be
    // reused until a vulkan GPU work depends on the following GL task is over.
    // So add it to the pending semaphores list, and they will be returned to
    // external semaphores pool when the next skia access is over.
    AddSemaphoresToPendingListOrRelease(std::move(external_semaphores));
  }
}

SharedImageBackingType ExternalVkImageBacking::GetType() const {
  return SharedImageBackingType::kExternalVkImage;
}

void ExternalVkImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  DCHECK(!in_fence);
}

bool ExternalVkImageBacking::UploadFromMemory(
    const std::vector<SkPixmap>& pixmaps) {
  if (!UploadToVkImage(pixmaps)) {
    return false;
  }

  latest_content_ = kInVkImage;

  // Also upload to GL texture if there is a separate one.
  if (use_separate_gl_texture() && !gl_textures_.empty()) {
    if (!UploadToGLTexture(pixmaps)) {
      return false;
    }
    latest_content_ |= kInGLTexture;
  }

  return true;
}

void ExternalVkImageBacking::AddSemaphoresToPendingListOrRelease(
    std::vector<ExternalSemaphore> semaphores) {
  constexpr size_t kMaxPendingSemaphores = 4;
  DCHECK_LE(pending_semaphores_.size(), kMaxPendingSemaphores);

#if DCHECK_IS_ON()
  for (auto& semaphore : semaphores)
    DCHECK(semaphore);
#endif
  while (pending_semaphores_.size() < kMaxPendingSemaphores &&
         !semaphores.empty()) {
    pending_semaphores_.push_back(std::move(semaphores.back()));
    semaphores.pop_back();
  }
  ReleaseSemaphoresWithFenceHelper(std::move(semaphores));
}

void ExternalVkImageBacking::ReleaseSemaphoresWithFenceHelper(
    std::vector<ExternalSemaphore> semaphores) {
  if (semaphores.empty()) {
    return;
  }

  // |semaphores| may contain VkSemephores which are submitted to queue for
  // signalling but have not been signalled. In that case, we have to release
  // them via fence helper to make sure all submitted GPU works is finished
  // before releasing them.
  fence_helper()->EnqueueCleanupTaskForSubmittedWork(base::BindOnce(
      [](scoped_refptr<SharedContextState> shared_context_state,
         std::vector<ExternalSemaphore>, VulkanDeviceQueue* device_queue,
         bool device_lost) {
        if (!gl::GLContext::GetCurrent()) {
          shared_context_state->MakeCurrent(/*surface=*/nullptr,
                                            /*needs_gl=*/true);
        }
      },
      context_state_, std::move(semaphores)));
}

scoped_refptr<gfx::NativePixmap> ExternalVkImageBacking::GetNativePixmap() {
  CHECK_EQ(vk_textures_.size(), 1u);
  return pixmap_;
}

gfx::GpuMemoryBufferHandle ExternalVkImageBacking::GetGpuMemoryBufferHandle() {
#if BUILDFLAG(IS_OZONE)
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::GpuMemoryBufferType::NATIVE_PIXMAP;
  handle.native_pixmap_handle = pixmap_->ExportHandle();
  return handle;
#else
  LOG(ERROR) << "Illegal access to GetGpuMemoryBufferHandle for non OZONE "
                "platforms from this backing.";
  NOTREACHED_IN_MIGRATION();
  return gfx::GpuMemoryBufferHandle();
#endif
}

void ExternalVkImageBacking::ReturnPendingSemaphoresWithFenceHelper(
    std::vector<ExternalSemaphore> semaphores) {
  std::move(semaphores.begin(), semaphores.end(),
            std::back_inserter(pending_semaphores_));
  external_semaphore_pool()->ReturnSemaphoresWithFenceHelper(
      std::move(pending_semaphores_));
  pending_semaphores_.clear();
}

std::unique_ptr<DawnImageRepresentation> ExternalVkImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    const wgpu::Device& wgpuDevice,
    wgpu::BackendType backend_type,
    std::vector<wgpu::TextureFormat> view_formats,
    scoped_refptr<SharedContextState> context_state) {
#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_DAWN)
  auto wgpu_format = ToDawnFormat(format());

  if (wgpu_format == wgpu::TextureFormat::Undefined) {
    DLOG(ERROR) << "Format not supported for Dawn";
    return nullptr;
  }

#if BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
  if (backend_type == wgpu::BackendType::OpenGLES) {
    auto image = ProduceGLTexturePassthrough(manager, tracker);
    return std::make_unique<DawnGLTextureRepresentation>(
        std::move(image), manager, this, tracker, wgpuDevice);
  }
#endif

  DCHECK_EQ(vk_textures_.size(), 1u);
  auto memory_fd = vk_textures_[0].vulkan_image->GetMemoryFd();
  if (!memory_fd.is_valid()) {
    return nullptr;
  }

  return std::make_unique<ExternalVkImageDawnImageRepresentation>(
      manager, this, tracker, wgpuDevice, wgpu_format, std::move(view_formats),
      std::move(memory_fd));
#else  // !BUILDFLAG(IS_LINUX) || !BUILDFLAG(USE_DAWN)
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
#endif
}

bool ExternalVkImageBacking::MakeGLContextCurrent() {
  if (gl::GLContext::GetCurrent()) {
    return true;
  }
  return context_state()->MakeCurrent(/*surface=*/nullptr, /*needs_gl=*/true);
}

bool ExternalVkImageBacking::ProduceGLTextureInternal(bool is_passthrough) {
  // It is not possible to import into GL when using external sampling.
  // Short-circuit out if this is requested (it should not be requested in
  // production, but might be in fuzzing flows).
  if (format().PrefersExternalSampler()) {
    LOG(ERROR)
        << "Importing textures with external sampling into GL is not possible";
    return false;
  }

  gl_textures_.reserve(vk_textures_.size());
  for (size_t plane = 0; plane < vk_textures_.size(); ++plane) {
    if (!CreateGLTexture(is_passthrough, plane)) {
      gl_textures_.clear();
      return false;
    }
  }

  return true;
}

bool ExternalVkImageBacking::CreateGLTexture(bool is_passthrough,
                                             size_t plane_index) {
  gl::GLApi* api = gl::g_current_gl_context;

  auto& vk_texture = vk_textures_[plane_index];
  auto& vulkan_image = vk_texture.vulkan_image;
  gfx::Size plane_size = vulkan_image->size();
  auto plane_format = GLTextureHolder::GetPlaneFormat(format(), plane_index);
  DCHECK_EQ(gl_textures_.size(), plane_index);
  auto& gl_texture = gl_textures_.emplace_back(plane_format, plane_size,
                                               is_passthrough, nullptr);

  std::optional<ScopedDedicatedMemoryObject> memory_object;
  if (!use_separate_gl_texture()) {
    GrVkImageInfo image_info = vk_texture.GetGrVkImageInfo();

#if BUILDFLAG(IS_POSIX)
    auto memory_fd = vulkan_image->GetMemoryFd();
    if (!memory_fd.is_valid()) {
      return false;
    }
    memory_object.emplace(api);
    api->glImportMemoryFdEXTFn(memory_object->id(), image_info.fAlloc.fSize,
                               GL_HANDLE_TYPE_OPAQUE_FD_EXT,
                               memory_fd.release());
#elif BUILDFLAG(IS_WIN)
    auto memory_handle = vulkan_image->GetMemoryHandle();
    if (!memory_handle.IsValid()) {
      return false;
    }
    memory_object.emplace(api);
    api->glImportMemoryWin32HandleEXTFn(
        memory_object->id(), image_info.fAlloc.fSize,
        GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, memory_handle.Take());
#elif BUILDFLAG(IS_FUCHSIA)
    zx::vmo vmo = vulkan_image->GetMemoryZirconHandle();
    if (!vmo) {
      return false;
    }
    memory_object.emplace(api);
    api->glImportMemoryZirconHandleANGLEFn(
        memory_object->id(), image_info.fAlloc.fSize,
        GL_HANDLE_TYPE_ZIRCON_VMO_ANGLE, vmo.release());
#else
#error Unsupported OS
#endif
  }

  GLFormatDesc format_desc =
      context_state()->GetGLFormatCaps().ToGLFormatDesc(format(), plane_index);

  GLuint texture_service_id = 0;
  api->glGenTexturesFn(1, &texture_service_id);
  gl::ScopedTextureBinder scoped_texture_binder(GL_TEXTURE_2D,
                                                texture_service_id);
  api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  if (use_separate_gl_texture()) {
    DCHECK(!memory_object);
    if (IsTexStorage2DAvailable()) {
      api->glTexStorage2DEXTFn(GL_TEXTURE_2D, 1,
                               format_desc.storage_internal_format,
                               plane_size.width(), plane_size.height());
    } else {
      api->glTexImage2DFn(GL_TEXTURE_2D, 0, format_desc.image_internal_format,
                          plane_size.width(), plane_size.height(), 0,
                          format_desc.data_format, format_desc.data_type,
                          nullptr);
    }
  } else {
    DCHECK(memory_object);
    // If ANGLE_memory_object_flags is supported, use that to communicate the
    // exact create and usage flags the image was created with.
    //
    // Currently, no extension structs are appended to VkImageCreateInfo::pNext
    // when creating the image, so communicate that information to ANGLE.  This
    // makes sure that ANGLE recreates the VkImage identically to Chromium.
    DCHECK_NE(vulkan_image->usage(), 0u);
    if (UseMinimalUsageFlags(context_state())) {
      api->glTexStorageMemFlags2DANGLEFn(
          GL_TEXTURE_2D, 1, format_desc.storage_internal_format,
          plane_size.width(), plane_size.height(), memory_object->id(), 0,
          vulkan_image->flags(), vulkan_image->usage(), nullptr);
    } else {
      api->glTexStorageMem2DEXTFn(
          GL_TEXTURE_2D, 1, format_desc.storage_internal_format,
          plane_size.width(), plane_size.height(), memory_object->id(), 0);
    }
  }

  if (is_passthrough) {
    auto texture = base::MakeRefCounted<gpu::gles2::TexturePassthrough>(
        texture_service_id, GL_TEXTURE_2D);
    gl_texture.InitializeWithTexture(format_desc, std::move(texture));
  } else {
    auto* texture = gles2::CreateGLES2TextureWithLightRef(texture_service_id,
                                                          GL_TEXTURE_2D);
    // If the backing is already cleared, no need to clear it again.
    gfx::Rect cleared_rect;
    if (IsCleared()) {
      cleared_rect = gfx::Rect(plane_size);
    }

    texture->SetLevelInfo(GL_TEXTURE_2D, 0, format_desc.storage_internal_format,
                          plane_size.width(), plane_size.height(), 1, 0,
                          format_desc.data_format, format_desc.data_type,
                          cleared_rect);
    texture->SetImmutable(true, true);
    gl_texture.InitializeWithTexture(format_desc, texture);
  }

  return true;
}

std::unique_ptr<GLTextureImageRepresentation>
ExternalVkImageBacking::ProduceGLTexture(SharedImageManager* manager,
                                         MemoryTypeTracker* tracker) {
  if (gl_textures_.empty()) {
    if (!ProduceGLTextureInternal(/*is_passthrough=*/false)) {
      return nullptr;
    }
  }

  std::vector<raw_ptr<gles2::Texture, VectorExperimental>> textures;
  textures.reserve(gl_textures_.size());
  for (auto& gl_texture : gl_textures_) {
    textures.push_back(gl_texture.texture());
  }

  return std::make_unique<ExternalVkImageGLRepresentation>(
      manager, this, tracker, std::move(textures));
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
ExternalVkImageBacking::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  if (gl_textures_.empty()) {
    if (!ProduceGLTextureInternal(/*is_passthrough=*/true)) {
      return nullptr;
    }
  }

  std::vector<scoped_refptr<gles2::TexturePassthrough>> textures;
  textures.reserve(gl_textures_.size());
  for (auto& gl_texture : gl_textures_) {
    textures.push_back(gl_texture.passthrough_texture());
  }

  return std::make_unique<ExternalVkImageGLPassthroughRepresentation>(
      manager, this, tracker, std::move(textures));
}

std::unique_ptr<SkiaGaneshImageRepresentation>
ExternalVkImageBacking::ProduceSkiaGanesh(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  if (context_state->GrContextIsVulkan()) {
    // If this backing type is used when vulkan is enabled, then SkiaRenderer
    // should also be using Vulkan.
    DCHECK_EQ(context_state_, context_state);
    return std::make_unique<ExternalVkImageSkiaImageRepresentation>(
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

std::unique_ptr<OverlayImageRepresentation>
ExternalVkImageBacking::ProduceOverlay(SharedImageManager* manager,
                                       MemoryTypeTracker* tracker) {
  return std::make_unique<ExternalVkImageOverlayImageRepresentation>(
      manager, this, tracker);
}

void ExternalVkImageBacking::UpdateContent(uint32_t content_flags) {
  // Only support one backing for now.
  DCHECK(content_flags == kInVkImage || content_flags == kInGLTexture);

  // There is no need to update content when there is only one texture.
  if (!use_separate_gl_texture())
    return;

  if ((latest_content_ & content_flags) == content_flags)
    return;

  if (content_flags == kInVkImage) {
    if ((latest_content_ & kInGLTexture)) {
      CopyPixelsFromGLTextureToVkImage();
      latest_content_ |= kInVkImage;
    }
  } else if (content_flags == kInGLTexture) {
    if (latest_content_ & kInVkImage) {
      CopyPixelsFromVkImageToGLTexture();
      latest_content_ |= kInGLTexture;
    }
  }
}

std::pair<std::vector<ExternalVkImageBacking::MapPlaneData>, size_t>
ExternalVkImageBacking::GetMapPlaneData() const {
  std::vector<MapPlaneData> data;
  size_t total_data_bytes = 0;
  size_t num_planes = vk_textures_.size();
  for (size_t plane = 0; plane < num_planes; ++plane) {
    data.push_back({AsSkImageInfo(plane), total_data_bytes});

    // Ensure that the start of the next plane is 4 byte aligned. For all
    // multi-planar formats the max texel block size is 4 bytes so this will
    // always satisfy the next planes alignment requirement.
    size_t plane_bytes = data.back().image_info.computeMinByteSize();
    base::bits::AlignUp<size_t>(plane_bytes, 4u);

    total_data_bytes += plane_bytes;
  }

  return {data, total_data_bytes};
}

void ExternalVkImageBacking::CopyPixelsFromGLTextureToVkImage() {
  DCHECK(use_separate_gl_texture());
  DCHECK_EQ(vk_textures_.size(), gl_textures_.size());

  // Make sure GrContext is not using GL. So we don't need reset GrContext
  DCHECK(!context_state_->GrContextIsGL());

  // Make sure a gl context is current, since textures are shared between all gl
  // contexts, we don't care which gl context is current.
  if (!MakeGLContextCurrent()) {
    return;
  }

  auto [plane_data, total_data_bytes] = GetMapPlaneData();
  VkBufferCreateInfo buffer_create_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = total_data_bytes,
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };

  VmaAllocator allocator =
      context_state()->vk_context_provider()->GetDeviceQueue()->vma_allocator();
  VkBuffer stage_buffer = VK_NULL_HANDLE;
  VmaAllocation stage_allocation = VK_NULL_HANDLE;
  VkResult result = vma::CreateBuffer(allocator, &buffer_create_info,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                      0, &stage_buffer, &stage_allocation);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vkCreateBuffer() failed." << result;
    return;
  }

  absl::Cleanup destroy_buffer = [&]() {
    vma::DestroyBuffer(allocator, stage_buffer, stage_allocation);
  };

  void* buffer = nullptr;
  result = vma::MapMemory(allocator, stage_allocation, &buffer);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vma::MapMemory() failed. " << result;
    return;
  }

  for (size_t plane = 0; plane < vk_textures_.size(); ++plane) {
    auto& sk_image_info = plane_data[plane].image_info;
    uint8_t* memory = static_cast<uint8_t*>(buffer) + plane_data[plane].offset;
    SkPixmap pixmap(sk_image_info, memory, sk_image_info.minRowBytes());

    if (!gl_textures_[plane].ReadbackToMemory(pixmap)) {
      DLOG(ERROR) << "GL readback failed";
      vma::UnmapMemory(allocator, stage_allocation);
      return;
    }
  }

  vma::UnmapMemory(allocator, stage_allocation);

  std::vector<ExternalSemaphore> external_semaphores;
  if (!BeginAccessInternal(/*readonly=*/false, &external_semaphores)) {
    DLOG(ERROR) << "BeginAccess() failed.";
    return;
  }

  // Everything was successful so `stage_buffer` + `stage_allocation` ownership
  // will be passed to EnqueueBufferCleanupForSubmittedWork().
  std::move(destroy_buffer).Cancel();

  auto command_buffer = command_pool_->CreatePrimaryCommandBuffer();
  CHECK(command_buffer);
  {
    ScopedSingleUseCommandBufferRecorder recorder(*command_buffer);

    for (size_t plane = 0; plane < vk_textures_.size(); ++plane) {
      GrVkImageInfo image_info = vk_textures_[plane].GetGrVkImageInfo();
      if (image_info.fImageLayout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        command_buffer->TransitionImageLayout(
            image_info.fImage, image_info.fImageLayout,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        GrBackendTextures::SetVkImageLayout(
            &vk_textures_[plane].backend_texture,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
      }

      auto& sk_image_info = plane_data[plane].image_info;
      command_buffer->CopyBufferToImage(
          stage_buffer, image_info.fImage, sk_image_info.width(),
          sk_image_info.height(), sk_image_info.width(), sk_image_info.height(),
          plane_data[plane].offset);
    }
  }

  if (!need_synchronization()) {
    DCHECK(external_semaphores.empty());
    command_buffer->Submit(0, nullptr, 0, nullptr);
    EndAccessInternal(/*readonly=*/false, ExternalSemaphore());

    fence_helper()->EnqueueVulkanObjectCleanupForSubmittedWork(
        std::move(command_buffer));
    fence_helper()->EnqueueBufferCleanupForSubmittedWork(stage_buffer,
                                                         stage_allocation);
    return;
  }

  std::vector<VkSemaphore> begin_access_semaphores;
  begin_access_semaphores.reserve(external_semaphores.size());
  for (auto& external_semaphore : external_semaphores) {
    begin_access_semaphores.emplace_back(external_semaphore.GetVkSemaphore());
  }

  auto end_access_semaphore = external_semaphore_pool()->GetOrCreateSemaphore();
  VkSemaphore vk_end_access_semaphore = end_access_semaphore.GetVkSemaphore();
  command_buffer->Submit(begin_access_semaphores.size(),
                         begin_access_semaphores.data(), 1,
                         &vk_end_access_semaphore);

  EndAccessInternal(/*readonly=*/false, std::move(end_access_semaphore));
  // |external_semaphores| have been waited on and can be reused when submitted
  // GPU work is done.
  ReturnPendingSemaphoresWithFenceHelper(std::move(external_semaphores));

  fence_helper()->EnqueueVulkanObjectCleanupForSubmittedWork(
      std::move(command_buffer));
  fence_helper()->EnqueueBufferCleanupForSubmittedWork(stage_buffer,
                                                       stage_allocation);
}

void ExternalVkImageBacking::CopyPixelsFromVkImageToGLTexture() {
  DCHECK(use_separate_gl_texture());
  DCHECK_EQ(vk_textures_.size(), gl_textures_.size());

  // Make sure GrContext is not using GL. So we don't need reset GrContext
  DCHECK(!context_state_->GrContextIsGL());

  // Make sure a gl context is current, since textures are shared between all gl
  // contexts, we don't care which gl context is current.
  if (!MakeGLContextCurrent()) {
    return;
  }

  auto [plane_data, total_data_bytes] = GetMapPlaneData();
  VkBufferCreateInfo buffer_create_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = total_data_bytes,
      .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };

  VmaAllocator allocator =
      context_state()->vk_context_provider()->GetDeviceQueue()->vma_allocator();
  VkBuffer stage_buffer = VK_NULL_HANDLE;
  VmaAllocation stage_allocation = VK_NULL_HANDLE;
  VkResult result = vma::CreateBuffer(allocator, &buffer_create_info,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                      0, &stage_buffer, &stage_allocation);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vkCreateBuffer() failed." << result;
    return;
  }

  absl::Cleanup destroy_buffer = [&]() {
    vma::DestroyBuffer(allocator, stage_buffer, stage_allocation);
  };

  // ReadPixelsWithCallback() is only called for separate texture.
  DCHECK(!need_synchronization());

  std::vector<ExternalSemaphore> external_semaphores;
  if (!BeginAccessInternal(/*readonly=*/true, &external_semaphores)) {
    DLOG(ERROR) << "BeginAccess() failed.";
    return;
  }
  DCHECK(external_semaphores.empty());

  auto command_buffer = command_pool_->CreatePrimaryCommandBuffer();
  CHECK(command_buffer);
  {
    ScopedSingleUseCommandBufferRecorder recorder(*command_buffer);

    for (size_t plane = 0; plane < vk_textures_.size(); ++plane) {
      GrVkImageInfo image_info = vk_textures_[plane].GetGrVkImageInfo();
      if (image_info.fImageLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        command_buffer->TransitionImageLayout(
            image_info.fImage, image_info.fImageLayout,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        GrBackendTextures::SetVkImageLayout(
            &vk_textures_[plane].backend_texture,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
      }

      auto& sk_image_info = plane_data[plane].image_info;
      command_buffer->CopyImageToBuffer(
          stage_buffer, image_info.fImage, sk_image_info.width(),
          sk_image_info.height(), sk_image_info.width(), sk_image_info.height(),
          plane_data[plane].offset);
    }
  }

  command_buffer->Submit(0, nullptr, 0, nullptr);
  command_buffer->Wait(UINT64_MAX);
  command_buffer->Destroy();
  EndAccessInternal(/*readonly=*/true, ExternalSemaphore());

  void* buffer = nullptr;
  result = vma::MapMemory(allocator, stage_allocation, &buffer);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vma::MapMemory() failed. " << result;
    return;
  }

  for (size_t plane = 0; plane < vk_textures_.size(); ++plane) {
    auto& sk_image_info = plane_data[plane].image_info;
    uint8_t* memory = static_cast<uint8_t*>(buffer) + plane_data[plane].offset;
    SkPixmap pixmap(sk_image_info, memory, sk_image_info.minRowBytes());
    if (!gl_textures_[plane].UploadFromMemory(pixmap)) {
      DLOG(ERROR) << "GL upload failed";
    }
  }

  vma::UnmapMemory(allocator, stage_allocation);
}

bool ExternalVkImageBacking::UploadToVkImage(
    const std::vector<SkPixmap>& pixmaps) {
  DCHECK_EQ(pixmaps.size(), vk_textures_.size());

  std::vector<ExternalSemaphore> external_semaphores;
  if (!BeginAccessInternal(/*readonly=*/false, &external_semaphores)) {
    DLOG(ERROR) << "BeginAccess() failed.";
    return false;
  }
  auto* gr_context = context_state_->gr_context();
  WaitSemaphoresOnGrContext(gr_context, &external_semaphores);

  bool success = true;
  for (size_t plane = 0; plane < vk_textures_.size(); ++plane) {
    if (!gr_context->updateBackendTexture(vk_textures_[plane].backend_texture,
                                          &pixmaps[plane],
                                          /*numLevels=*/1, nullptr, nullptr)) {
      success = false;
      DLOG(ERROR) << "updateBackendTexture() failed.";
    }
  }

  if (!need_synchronization()) {
    DCHECK(external_semaphores.empty());
    EndAccessInternal(/*readonly=*/false, ExternalSemaphore());
    return success;
  }

  gr_context->flush(GrFlushInfo());
  for (auto& vk_texture : vk_textures_) {
    gr_context->setBackendTextureState(
        vk_texture.backend_texture,
        skgpu::MutableTextureStates::MakeVulkan(
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_QUEUE_FAMILY_EXTERNAL));
  }

  auto end_access_semaphore = external_semaphore_pool()->GetOrCreateSemaphore();
  VkSemaphore vk_end_access_semaphore = end_access_semaphore.GetVkSemaphore();
  GrBackendSemaphore end_access_backend_semaphore =
      GrBackendSemaphores::MakeVk(vk_end_access_semaphore);
  GrFlushInfo flush_info = {
      .fNumSemaphores = 1,
      .fSignalSemaphores = &end_access_backend_semaphore,
  };
  gr_context->flush(flush_info);

  // Submit so the |end_access_semaphore| is ready for waiting.
  gr_context->submit();

  EndAccessInternal(/*readonly=*/false, std::move(end_access_semaphore));
  // |external_semaphores| have been waited on and can be reused when submitted
  // GPU work is done.
  ReturnPendingSemaphoresWithFenceHelper(std::move(external_semaphores));
  return success;
}

bool ExternalVkImageBacking::UploadToGLTexture(
    const std::vector<SkPixmap>& pixmaps) {
  DCHECK(use_separate_gl_texture());
  DCHECK_EQ(gl_textures_.size(), pixmaps.size());

  // Make sure a gl context is current, since textures are shared between all gl
  // contexts, we don't care which gl context is current.
  if (!MakeGLContextCurrent()) {
    return false;
  }

  for (size_t i = 0; i < gl_textures_.size(); ++i) {
    if (!gl_textures_[i].UploadFromMemory(pixmaps[i])) {
      return false;
    }
  }
  return true;
}

bool ExternalVkImageBacking::BeginAccessInternal(
    bool readonly,
    std::vector<ExternalSemaphore>* external_semaphores) {
  DCHECK(external_semaphores);
  DCHECK(external_semaphores->empty());
  if (is_write_in_progress_) {
    DLOG(ERROR) << "Unable to begin read or write access because another write "
                   "access is in progress";
    return false;
  }

  if (reads_in_progress_ && !readonly) {
    DLOG(ERROR)
        << "Unable to begin write access because a read access is in progress";
    return false;
  }

  if (readonly) {
    DLOG_IF(ERROR, reads_in_progress_)
        << "Concurrent reading may cause problem.";
    ++reads_in_progress_;
    // If a shared image is read repeatedly without any write access,
    // |read_semaphores_| will never be consumed and released, and then
    // chrome will run out of file descriptors. To avoid this problem, we wait
    // on read semaphores for readonly access too. And in most cases, a shared
    // image is only read from one vulkan device queue, so it should not have
    // performance impact.
    // TODO(penghuang): avoid waiting on read semaphores.
    *external_semaphores = std::move(read_semaphores_);
    read_semaphores_.clear();

    // A semaphore will become unsignaled, when it has been signaled and waited,
    // so it is not safe to reuse it.
    if (write_semaphore_)
      external_semaphores->push_back(std::move(write_semaphore_));
  } else {
    is_write_in_progress_ = true;
    *external_semaphores = std::move(read_semaphores_);
    read_semaphores_.clear();
    if (write_semaphore_)
      external_semaphores->push_back(std::move(write_semaphore_));
  }
  return true;
}

void ExternalVkImageBacking::EndAccessInternal(
    bool readonly,
    ExternalSemaphore external_semaphore) {
  if (readonly) {
    DCHECK_GT(reads_in_progress_, 0u);
    --reads_in_progress_;
  } else {
    DCHECK(is_write_in_progress_);
    is_write_in_progress_ = false;
  }

  if (need_synchronization() && external_semaphore) {
    DCHECK(!is_write_in_progress_);
    if (readonly) {
      read_semaphores_.push_back(std::move(external_semaphore));
    } else {
      DCHECK(!write_semaphore_);
      DCHECK(read_semaphores_.empty());
      write_semaphore_ = std::move(external_semaphore);
    }
  } else {
    DCHECK(!external_semaphore);
  }
}

}  // namespace gpu
