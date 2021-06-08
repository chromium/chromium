// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_scoped_hardware_buffer_fence_sync.h"

#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/posix/eintr_wrapper.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/abstract_texture.h"
#include "gpu/command_buffer/service/ahardwarebuffer_utils.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image_representation_skia_gl.h"
#include "gpu/command_buffer/service/shared_image_representation_skia_vk_android.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/command_buffer/service/texture_owner.h"
#include "gpu/ipc/common/android/android_image_reader_utils.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_util.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"

namespace gpu {

class SharedImageRepresentationOverlayScopedHardwareBufferFenceSync
    : public SharedImageRepresentationOverlay {
 public:
  SharedImageRepresentationOverlayScopedHardwareBufferFenceSync(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker)
      : SharedImageRepresentationOverlay(manager, backing, tracker) {}

  ~SharedImageRepresentationOverlayScopedHardwareBufferFenceSync() override {
    DCHECK(!hardware_buffer_);
  }

 private:
  SharedImageBackingScopedHardwareBufferFenceSync* ahb_backing() {
    return static_cast<SharedImageBackingScopedHardwareBufferFenceSync*>(
        backing());
  }

  void NotifyOverlayPromotion(bool promotion,
                              const gfx::Rect& bounds) override {
    NOTREACHED();
  }

  bool BeginReadAccess(std::vector<gfx::GpuFence>* acquire_fences) override {
    gfx::GpuFenceHandle begin_read_handle;
    hardware_buffer_ = ahb_backing()->BeginOverlayAccess(begin_read_handle);
    if (!begin_read_handle.is_null())
      acquire_fences->emplace_back(std::move(begin_read_handle));
    return true;
  }

  void EndReadAccess(gfx::GpuFenceHandle release_fence) override {
    ahb_backing()->EndOverlayAccess(std::move(release_fence));
    hardware_buffer_ = nullptr;
  }

  AHardwareBuffer* GetAHardwareBuffer() override { return hardware_buffer_; }

  gl::GLImage* GetGLImage() override {
    NOTREACHED();
    return nullptr;
  }

  AHardwareBuffer* hardware_buffer_ = nullptr;
};

SharedImageBackingScopedHardwareBufferFenceSync::
    SharedImageBackingScopedHardwareBufferFenceSync(
        std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
            scoped_hardware_buffer,
        const Mailbox& mailbox,
        viz::ResourceFormat format,
        const gfx::Size& size,
        const gfx::ColorSpace& color_space,
        GrSurfaceOrigin surface_origin,
        SkAlphaType alpha_type,
        uint32_t usage,
        bool is_thread_safe)
    : SharedImageBackingAndroid(
          mailbox,
          format,
          size,
          color_space,
          surface_origin,
          alpha_type,
          usage,
          viz::ResourceSizes::UncheckedSizeInBytes<size_t>(size, format),
          is_thread_safe,
          base::ScopedFD()),
      scoped_hardware_buffer_(std::move(scoped_hardware_buffer)) {
  // This backing is not thread safe and should not be accessed by multiple
  // thread at once.
  DCHECK(!is_thread_safe);
  DCHECK(scoped_hardware_buffer_);
  ahb_read_fence_ = scoped_hardware_buffer_->TakeFence();
}

SharedImageBackingScopedHardwareBufferFenceSync::
    ~SharedImageBackingScopedHardwareBufferFenceSync() {}

gfx::Rect SharedImageBackingScopedHardwareBufferFenceSync::ClearedRect() const {
  // SharedImageBackingScopedHardwareBufferFenceSync objects are always created
  // from pre-initialized hardware buffers.( eg: images provided by the media
  // decoder's AImageReader.) Always treat these as cleared (return the full
  // rectangle).
  return gfx::Rect(size());
}

void SharedImageBackingScopedHardwareBufferFenceSync::SetClearedRect(
    const gfx::Rect& cleared_rect) {}

void SharedImageBackingScopedHardwareBufferFenceSync::Update(
    std::unique_ptr<gfx::GpuFence> in_fence) {
  DCHECK(!in_fence);
}

bool SharedImageBackingScopedHardwareBufferFenceSync::ProduceLegacyMailbox(
    MailboxManager* mailbox_manager) {
  // This backing do not support legacy mailbox system.
  NOTREACHED();
  return false;
}

size_t
SharedImageBackingScopedHardwareBufferFenceSync::EstimatedSizeForMemTracking()
    const {
  return estimated_size();
}

bool SharedImageBackingScopedHardwareBufferFenceSync::BeginGLReadAccess() {
  // Wait on the |ahb_read_fence_| to make sure buffer is ready before the read.
  // Duping the fence so that we can issue waits for each access.
  return InsertEglFenceAndWait(
      base::ScopedFD(HANDLE_EINTR(dup(ahb_read_fence_.get()))));
}

void SharedImageBackingScopedHardwareBufferFenceSync::EndGLReadAccess() {
  base::ScopedFD sync_fd = CreateEglFenceAndExportFd();

  // Set the end read access fence on the scoped hardware buffer so that it can
  // be destroyed once this fence is waited upon.
  scoped_hardware_buffer_->SetReadFence(std::move(sync_fd), true);
}

void SharedImageBackingScopedHardwareBufferFenceSync::EndSkiaReadAccess() {
  scoped_hardware_buffer_->SetReadFence(TakeReadFence(), true);
}

// Representation of SharedImageScopedHardwareBufferFenceSync as a GL Texture.
class SharedImageRepresentationGLTextureScopedHardwareBufferFenceSync
    : public SharedImageRepresentationGLTexture {
 public:
  SharedImageRepresentationGLTextureScopedHardwareBufferFenceSync(
      SharedImageManager* manager,
      SharedImageBackingScopedHardwareBufferFenceSync* backing,
      MemoryTypeTracker* tracker,
      gles2::Texture* texture)
      : SharedImageRepresentationGLTexture(manager, backing, tracker),
        texture_(texture) {
    DCHECK(texture_);
  }

  ~SharedImageRepresentationGLTextureScopedHardwareBufferFenceSync() override {
    if (texture_)
      texture_->RemoveLightweightRef(has_context());
  }

  gles2::Texture* GetTexture() override { return texture_; }

  bool BeginAccess(GLenum mode) override {
    // This representation only supports read access.
    DCHECK_EQ(mode,
              static_cast<GLenum>(GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM));

    return ahb_backing()->BeginGLReadAccess();
  }

  void EndAccess() override { ahb_backing()->EndGLReadAccess(); }

 private:
  SharedImageBackingScopedHardwareBufferFenceSync* ahb_backing() {
    auto* ahb_backing =
        static_cast<SharedImageBackingScopedHardwareBufferFenceSync*>(
            backing());
    DCHECK(ahb_backing);
    return ahb_backing;
  }

  gles2::Texture* texture_;

  DISALLOW_COPY_AND_ASSIGN(
      SharedImageRepresentationGLTextureScopedHardwareBufferFenceSync);
};

class SharedImageRepresentationGLTexturePassthroughScopedHardwareBufferFenceSync
    : public SharedImageRepresentationGLTexturePassthrough {
 public:
  SharedImageRepresentationGLTexturePassthroughScopedHardwareBufferFenceSync(
      SharedImageManager* manager,
      SharedImageBackingScopedHardwareBufferFenceSync* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<gles2::TexturePassthrough> texture)
      : SharedImageRepresentationGLTexturePassthrough(manager,
                                                      backing,
                                                      tracker),
        texture_(std::move(texture)) {
    // TODO(https://crbug.com/1172769): Remove this CHECK.
    CHECK(texture_);
  }

  ~SharedImageRepresentationGLTexturePassthroughScopedHardwareBufferFenceSync()
      override {
    if (!has_context())
      texture_->MarkContextLost();
  }

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough()
      override {
    return texture_;
  }

  bool BeginAccess(GLenum mode) override {
    // This representation only supports read access.
    DCHECK_EQ(mode,
              static_cast<GLenum>(GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM));

    return ahb_backing()->BeginGLReadAccess();
  }

  void EndAccess() override { ahb_backing()->EndGLReadAccess(); }

 private:
  SharedImageBackingScopedHardwareBufferFenceSync* ahb_backing() {
    auto* ahb_backing =
        static_cast<SharedImageBackingScopedHardwareBufferFenceSync*>(
            backing());
    DCHECK(ahb_backing);
    return ahb_backing;
  }

  scoped_refptr<gles2::TexturePassthrough> texture_;

  DISALLOW_COPY_AND_ASSIGN(
      SharedImageRepresentationGLTexturePassthroughScopedHardwareBufferFenceSync);
};

class SharedImageRepresentationSkiaVkScopedHardwareBufferFenceSync
    : public SharedImageRepresentationSkiaVkAndroid {
 public:
  SharedImageRepresentationSkiaVkScopedHardwareBufferFenceSync(
      SharedImageManager* manager,
      SharedImageBackingAndroid* backing,
      scoped_refptr<SharedContextState> context_state,
      MemoryTypeTracker* tracker,
      std::unique_ptr<VulkanImage> vulkan_image,
      base::ScopedFD init_read_fence)
      : SharedImageRepresentationSkiaVkAndroid(manager,
                                               backing,
                                               std::move(context_state),
                                               tracker) {
    DCHECK(vulkan_image);

    vulkan_image_ = std::move(vulkan_image);
    init_read_fence_ = std::move(init_read_fence);

    promise_texture_ = SkPromiseImageTexture::Make(
        GrBackendTexture(size().width(), size().height(),
                         CreateGrVkImageInfo(vulkan_image_.get())));
    DCHECK(promise_texture_);
  }

  sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    // Writes are not intended to used for this backing.
    NOTIMPLEMENTED();
    return nullptr;
  }

  void EndWriteAccess(sk_sp<SkSurface> surface) override { NOTIMPLEMENTED(); }

  // BeginReadAccess implementation was not required as base class
  // implementation was enough.
  void EndReadAccess() override {
    SharedImageRepresentationSkiaVkAndroid::EndReadAccess();
    ahb_backing()->EndSkiaReadAccess();
  }

 private:
  SharedImageBackingScopedHardwareBufferFenceSync* ahb_backing() {
    auto* ahb_backing =
        static_cast<SharedImageBackingScopedHardwareBufferFenceSync*>(
            backing());
    DCHECK(ahb_backing);

    return ahb_backing;
  }
};

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageBackingScopedHardwareBufferFenceSync::ProduceGLTexture(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  return GenGLTextureRepresentation(manager, tracker);
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
SharedImageBackingScopedHardwareBufferFenceSync::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  return GenGLTexturePassthroughRepresentation(manager, tracker);
  ;
}

std::unique_ptr<SharedImageRepresentationSkia>
SharedImageBackingScopedHardwareBufferFenceSync::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  DCHECK(context_state);

  if (context_state->GrContextIsVulkan()) {
    // Create a new handle from the buffer which adds a reference to the buffer
    // instead of taking the ownership.
    auto ahb_handle = base::android::ScopedHardwareBufferHandle::Create(
        scoped_hardware_buffer_->buffer());

    auto vulkan_image = CreateVkImageFromAhbHandle(
        std::move(ahb_handle), context_state.get(), size(), format());
    if (!vulkan_image)
      return nullptr;

    auto read_fence = base::ScopedFD(HANDLE_EINTR(dup(ahb_read_fence_.get())));
    return std::make_unique<
        SharedImageRepresentationSkiaVkScopedHardwareBufferFenceSync>(
        manager, this, std::move(context_state), tracker,
        std::move(vulkan_image), std::move(read_fence));
  }

  DCHECK(context_state->GrContextIsGL());

  std::unique_ptr<SharedImageRepresentationGLTextureBase> gl_representation;
  if (context_state->feature_info()->is_passthrough_cmd_decoder()) {
    gl_representation = GenGLTexturePassthroughRepresentation(manager, tracker);
  } else {
    gl_representation = GenGLTextureRepresentation(manager, tracker);
  }
  if (!gl_representation)
    return nullptr;
  return SharedImageRepresentationSkiaGL::Create(std::move(gl_representation),
                                                 std::move(context_state),
                                                 manager, this, tracker);
}

std::unique_ptr<SharedImageRepresentationGLTextureScopedHardwareBufferFenceSync>
SharedImageBackingScopedHardwareBufferFenceSync::GenGLTextureRepresentation(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  auto* texture =
      GenGLTexture(scoped_hardware_buffer_->buffer(), GL_TEXTURE_EXTERNAL_OES,
                   color_space(), size(), estimated_size(), ClearedRect());
  if (!texture)
    return nullptr;
  return std::make_unique<
      SharedImageRepresentationGLTextureScopedHardwareBufferFenceSync>(
      manager, this, tracker, texture);
}

std::unique_ptr<
    SharedImageRepresentationGLTexturePassthroughScopedHardwareBufferFenceSync>
SharedImageBackingScopedHardwareBufferFenceSync::
    GenGLTexturePassthroughRepresentation(SharedImageManager* manager,
                                          MemoryTypeTracker* tracker) {
  auto texture = GenGLTexturePassthrough(
      scoped_hardware_buffer_->buffer(), GL_TEXTURE_EXTERNAL_OES, color_space(),
      size(), estimated_size(), ClearedRect());
  if (!texture)
    return nullptr;
  return std::make_unique<
      SharedImageRepresentationGLTexturePassthroughScopedHardwareBufferFenceSync>(
      manager, this, tracker, std::move(texture));
}

AHardwareBuffer*
SharedImageBackingScopedHardwareBufferFenceSync::BeginOverlayAccess(
    gfx::GpuFenceHandle& begin_read_fence) {
  AutoLock auto_lock(this);

  DCHECK(!is_overlay_accessing_);

  if (is_writing_) {
    LOG(ERROR)
        << "BeginOverlayAccess should only be called when there are no writers";
    return nullptr;
  }

  if (ahb_read_fence_.is_valid()) {
    base::ScopedFD fence_fd(HANDLE_EINTR(dup(ahb_read_fence_.get())));
    begin_read_fence.owned_fd = std::move(fence_fd);
  }

  is_overlay_accessing_ = true;
  return scoped_hardware_buffer_->buffer();
}

void SharedImageBackingScopedHardwareBufferFenceSync::EndOverlayAccess(
    gfx::GpuFenceHandle release_fence) {
  AutoLock auto_lock(this);

  DCHECK(is_overlay_accessing_);
  is_overlay_accessing_ = false;

  if (!release_fence.is_null()) {
    scoped_hardware_buffer_->SetReadFence(std::move(release_fence.owned_fd),
                                          true);
  }
}

std::unique_ptr<SharedImageRepresentationOverlay>
SharedImageBackingScopedHardwareBufferFenceSync::ProduceOverlay(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  return std::make_unique<
      SharedImageRepresentationOverlayScopedHardwareBufferFenceSync>(
      manager, this, tracker);
}

}  // namespace gpu
