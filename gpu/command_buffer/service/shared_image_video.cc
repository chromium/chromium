// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_video.h"

#include <utility>

#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/android/scoped_hardware_buffer_handle.h"
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
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_util.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"

namespace gpu {

SharedImageVideo::SharedImageVideo(
    const Mailbox& mailbox,
    const gfx::Size& size,
    const gfx::ColorSpace color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    scoped_refptr<StreamTextureSharedImageInterface> stream_texture_sii,
    std::unique_ptr<gles2::AbstractTexture> abstract_texture,
    scoped_refptr<SharedContextState> context_state,
    bool is_thread_safe)
    : SharedImageBackingAndroid(
          mailbox,
          viz::RGBA_8888,
          size,
          color_space,
          surface_origin,
          alpha_type,
          (SHARED_IMAGE_USAGE_DISPLAY | SHARED_IMAGE_USAGE_GLES2),
          viz::ResourceSizes::UncheckedSizeInBytes<size_t>(size,
                                                           viz::RGBA_8888),
          is_thread_safe,
          base::ScopedFD()),
      stream_texture_sii_(std::move(stream_texture_sii)),
      abstract_texture_(std::move(abstract_texture)),
      context_state_(std::move(context_state)) {
  DCHECK(stream_texture_sii_);
  DCHECK(context_state_);

  // Currently this backing is not thread safe.
  DCHECK(!is_thread_safe);
  context_state_->AddContextLostObserver(this);
}

SharedImageVideo::~SharedImageVideo() {
  stream_texture_sii_->ReleaseResources();
  if (context_state_)
    context_state_->RemoveContextLostObserver(this);
}

gfx::Rect SharedImageVideo::ClearedRect() const {
  // SharedImageVideo objects are always created from pre-initialized textures
  // provided by the media decoder. Always treat these as cleared (return the
  // full rectangle).
  return gfx::Rect(size());
}

void SharedImageVideo::SetClearedRect(const gfx::Rect& cleared_rect) {}

void SharedImageVideo::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  DCHECK(!in_fence);
}

bool SharedImageVideo::ProduceLegacyMailbox(MailboxManager* mailbox_manager) {
  DCHECK(abstract_texture_);
  mailbox_manager->ProduceTexture(mailbox(),
                                  abstract_texture_->GetTextureBase());
  return true;
}

size_t SharedImageVideo::EstimatedSizeForMemTracking() const {
  // This backing contributes to gpu memory only if its bound to the texture and
  // not when the backing is created.
  return stream_texture_sii_->IsUsingGpuMemory() ? estimated_size() : 0;
}

void SharedImageVideo::OnContextLost() {
  // We release codec buffers when shared image context is lost. This is because
  // texture owner's texture was created on shared context. Once shared context
  // is lost, no one should try to use that texture.
  stream_texture_sii_->ReleaseResources();
  context_state_->RemoveContextLostObserver(this);
  context_state_ = nullptr;
}

base::Optional<VulkanYCbCrInfo> SharedImageVideo::GetYcbcrInfo(
    TextureOwner* texture_owner,
    scoped_refptr<SharedContextState> context_state) {
  // For non-vulkan context, return null.
  if (!context_state->GrContextIsVulkan())
    return base::nullopt;

  // GetAHardwareBuffer() renders the latest image and gets AHardwareBuffer
  // from it.
  auto scoped_hardware_buffer = texture_owner->GetAHardwareBuffer();
  if (!scoped_hardware_buffer) {
    return base::nullopt;
  }

  DCHECK(scoped_hardware_buffer->buffer());
  auto* context_provider = context_state->vk_context_provider();
  VulkanImplementation* vk_implementation =
      context_provider->GetVulkanImplementation();
  VkDevice vk_device = context_provider->GetDeviceQueue()->GetVulkanDevice();

  VulkanYCbCrInfo ycbcr_info;
  if (!vk_implementation->GetSamplerYcbcrConversionInfo(
          vk_device, scoped_hardware_buffer->TakeBuffer(), &ycbcr_info)) {
    LOG(ERROR) << "Failed to get the ycbcr info.";
    return base::nullopt;
  }
  return base::Optional<VulkanYCbCrInfo>(ycbcr_info);
}

std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
SharedImageVideo::GetAHardwareBuffer() {
  DCHECK(stream_texture_sii_);
  return stream_texture_sii_->GetAHardwareBuffer();
}

// Representation of SharedImageVideo as a GL Texture.
class SharedImageRepresentationGLTextureVideo
    : public SharedImageRepresentationGLTexture {
 public:
  SharedImageRepresentationGLTextureVideo(SharedImageManager* manager,
                                          SharedImageVideo* backing,
                                          MemoryTypeTracker* tracker,
                                          gles2::Texture* texture)
      : SharedImageRepresentationGLTexture(manager, backing, tracker),
        texture_(texture) {}

  gles2::Texture* GetTexture() override { return texture_; }

  bool BeginAccess(GLenum mode) override {
    // This representation should only be called for read or overlay.
    DCHECK(mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM ||
           mode == GL_SHARED_IMAGE_ACCESS_MODE_OVERLAY_CHROMIUM);

    auto* video_backing = static_cast<SharedImageVideo*>(backing());
    video_backing->BeginGLReadAccess();
    return true;
  }

  void EndAccess() override {}

 private:
  gles2::Texture* texture_;

  DISALLOW_COPY_AND_ASSIGN(SharedImageRepresentationGLTextureVideo);
};

// Representation of SharedImageVideo as a GL Texture.
class SharedImageRepresentationGLTexturePassthroughVideo
    : public SharedImageRepresentationGLTexturePassthrough {
 public:
  SharedImageRepresentationGLTexturePassthroughVideo(
      SharedImageManager* manager,
      SharedImageVideo* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<gles2::TexturePassthrough> texture)
      : SharedImageRepresentationGLTexturePassthrough(manager,
                                                      backing,
                                                      tracker),
        texture_(std::move(texture)) {}

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough()
      override {
    return texture_;
  }

  bool BeginAccess(GLenum mode) override {
    // This representation should only be called for read or overlay.
    DCHECK(mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM ||
           mode == GL_SHARED_IMAGE_ACCESS_MODE_OVERLAY_CHROMIUM);

    auto* video_backing = static_cast<SharedImageVideo*>(backing());
    video_backing->BeginGLReadAccess();
    return true;
  }

  void EndAccess() override {}

 private:
  scoped_refptr<gles2::TexturePassthrough> texture_;

  DISALLOW_COPY_AND_ASSIGN(SharedImageRepresentationGLTexturePassthroughVideo);
};

class SharedImageRepresentationVideoSkiaVk
    : public SharedImageRepresentationSkiaVkAndroid {
 public:
  SharedImageRepresentationVideoSkiaVk(
      SharedImageManager* manager,
      SharedImageBackingAndroid* backing,
      scoped_refptr<SharedContextState> context_state,
      MemoryTypeTracker* tracker)
      : SharedImageRepresentationSkiaVkAndroid(manager,
                                               backing,
                                               std::move(context_state),
                                               tracker) {}

  sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    // Writes are not intended to used for video backed representations.
    NOTIMPLEMENTED();
    return nullptr;
  }

  void EndWriteAccess(sk_sp<SkSurface> surface) override { NOTIMPLEMENTED(); }

  sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    DCHECK(!scoped_hardware_buffer_);
    auto* video_backing = static_cast<SharedImageVideo*>(backing());
    DCHECK(video_backing);
    auto* stream_texture_sii = video_backing->stream_texture_sii_.get();

    // GetAHardwareBuffer() renders the latest image and gets AHardwareBuffer
    // from it.
    scoped_hardware_buffer_ = stream_texture_sii->GetAHardwareBuffer();
    if (!scoped_hardware_buffer_) {
      LOG(ERROR) << "Failed to get the hardware buffer.";
      return nullptr;
    }
    DCHECK(scoped_hardware_buffer_->buffer());

    // Wait on the sync fd attached to the buffer to make sure buffer is
    // ready before the read. This is done by inserting the sync fd semaphore
    // into begin_semaphore vector which client will wait on.
    init_read_fence_ = scoped_hardware_buffer_->TakeFence();

    if (!vulkan_image_) {
      DCHECK(!promise_texture_);

      vulkan_image_ =
          CreateVkImageFromAhbHandle(scoped_hardware_buffer_->TakeBuffer(),
                                     context_state(), size(), format());
      if (!vulkan_image_)
        return nullptr;

      // We always use VK_IMAGE_TILING_OPTIMAL while creating the vk image in
      // VulkanImplementationAndroid::CreateVkImageAndImportAHB. Hence pass the
      // tiling parameter as VK_IMAGE_TILING_OPTIMAL to below call rather than
      // passing |vk_image_info.tiling|. This is also to ensure that the promise
      // image created here at [1] as well the fullfil image created via the
      // current function call are consistent and both are using
      // VK_IMAGE_TILING_OPTIMAL. [1] -
      // https://cs.chromium.org/chromium/src/components/viz/service/display_embedder/skia_output_surface_impl.cc?rcl=db5ffd448ba5d66d9d3c5c099754e5067c752465&l=789.
      DCHECK_EQ(static_cast<int32_t>(vulkan_image_->image_tiling()),
                static_cast<int32_t>(VK_IMAGE_TILING_OPTIMAL));

      // TODO(bsalomon): Determine whether it makes sense to attempt to reuse
      // this if the vk_info stays the same on subsequent calls.
      promise_texture_ = SkPromiseImageTexture::Make(
          GrBackendTexture(size().width(), size().height(),
                           CreateGrVkImageInfo(vulkan_image_.get())));
      DCHECK(promise_texture_);
    }

    return SharedImageRepresentationSkiaVkAndroid::BeginReadAccess(
        begin_semaphores, end_semaphores, end_state);
  }

  void EndReadAccess() override {
    DCHECK(scoped_hardware_buffer_);

    SharedImageRepresentationSkiaVkAndroid::EndReadAccess();

    // Pass the end read access sync fd to the scoped hardware buffer. This will
    // make sure that the AImage associated with the hardware buffer will be
    // deleted only when the read access is ending.
    scoped_hardware_buffer_->SetReadFence(android_backing()->TakeReadFence(),
                                          true);
    scoped_hardware_buffer_ = nullptr;
  }

 private:
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
      scoped_hardware_buffer_;
};

// TODO(vikassoni): Currently GLRenderer doesn't support overlays with shared
// image. Add support for overlays in GLRenderer as well as overlay
// representations of shared image.
std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageVideo::ProduceGLTexture(SharedImageManager* manager,
                                   MemoryTypeTracker* tracker) {
  // For (old) overlays, we don't have a texture owner, but overlay promotion
  // might not happen for some reasons. In that case, it will try to draw
  // which should result in no image.
  if (!stream_texture_sii_->HasTextureOwner())
    return nullptr;
  // TODO(vikassoni): We would want to give the TextureOwner's underlying
  // Texture, but it was not set with the correct size. The AbstractTexture,
  // that we use for legacy mailbox, is correctly set.
  auto* texture =
      gles2::Texture::CheckedCast(abstract_texture_->GetTextureBase());
  DCHECK(texture);

  return std::make_unique<SharedImageRepresentationGLTextureVideo>(
      manager, this, tracker, texture);
}

// TODO(vikassoni): Currently GLRenderer doesn't support overlays with shared
// image. Add support for overlays in GLRenderer as well as overlay
// representations of shared image.
std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
SharedImageVideo::ProduceGLTexturePassthrough(SharedImageManager* manager,
                                              MemoryTypeTracker* tracker) {
  // For (old) overlays, we don't have a texture owner, but overlay promotion
  // might not happen for some reasons. In that case, it will try to draw
  // which should result in no image.
  if (!stream_texture_sii_->HasTextureOwner())
    return nullptr;
  // TODO(vikassoni): We would want to give the TextureOwner's underlying
  // Texture, but it was not set with the correct size. The AbstractTexture,
  // that we use for legacy mailbox, is correctly set.
  scoped_refptr<gles2::TexturePassthrough> texture =
      gles2::TexturePassthrough::CheckedCast(
          abstract_texture_->GetTextureBase());
  DCHECK(texture);

  return std::make_unique<SharedImageRepresentationGLTexturePassthroughVideo>(
      manager, this, tracker, std::move(texture));
}

// Currently SkiaRenderer doesn't support overlays.
std::unique_ptr<SharedImageRepresentationSkia> SharedImageVideo::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  DCHECK(context_state);

  // For (old) overlays, we don't have a texture owner, but overlay promotion
  // might not happen for some reasons. In that case, it will try to draw
  // which should result in no image.
  if (!stream_texture_sii_->HasTextureOwner())
    return nullptr;

  if (context_state->GrContextIsVulkan()) {
    return std::make_unique<SharedImageRepresentationVideoSkiaVk>(
        manager, this, std::move(context_state), tracker);
  }

  DCHECK(context_state->GrContextIsGL());
  auto* texture_base = stream_texture_sii_->GetTextureBase();
  DCHECK(texture_base);

  // In GL mode, create the SharedImageRepresentationGLTexture*Video
  // representation to use with SharedImageRepresentationVideoSkiaGL.
  std::unique_ptr<gpu::SharedImageRepresentationGLTextureBase>
      gl_representation;
  if (texture_base->GetType() == gpu::TextureBase::Type::kValidated) {
    gl_representation =
        std::make_unique<SharedImageRepresentationGLTextureVideo>(
            manager, this, tracker, gles2::Texture::CheckedCast(texture_base));
  } else {
    gl_representation =
        std::make_unique<SharedImageRepresentationGLTexturePassthroughVideo>(
            manager, this, tracker,
            gles2::TexturePassthrough::CheckedCast(texture_base));
  }

  return SharedImageRepresentationSkiaGL::Create(std::move(gl_representation),
                                                 std::move(context_state),
                                                 manager, this, tracker);
}

void SharedImageVideo::BeginGLReadAccess() {
  // Render the codec image.
  stream_texture_sii_->UpdateAndBindTexImage();
}

// Representation of SharedImageVideo as an overlay plane.
class SharedImageRepresentationOverlayVideo
    : public gpu::SharedImageRepresentationOverlay {
 public:
  SharedImageRepresentationOverlayVideo(gpu::SharedImageManager* manager,
                                        SharedImageVideo* backing,
                                        gpu::MemoryTypeTracker* tracker)
      : gpu::SharedImageRepresentationOverlay(manager, backing, tracker),
        stream_image_(backing->stream_texture_sii_) {}

 protected:
  bool BeginReadAccess() override {
    // A |CodecImage| is already in a SurfaceView, render content to the
    // overlay.
    if (!stream_image_->HasTextureOwner()) {
      TRACE_EVENT0("media",
                   "SharedImageRepresentationOverlayVideo::BeginReadAccess");
      stream_image_->RenderToOverlay();
    }
    return true;
  }

  void EndReadAccess() override {}

  gl::GLImage* GetGLImage() override {
    DCHECK(stream_image_->HasTextureOwner())
        << "The backing is already in a SurfaceView!";
    return stream_image_.get();
  }

  std::unique_ptr<gfx::GpuFence> GetReadFence() override { return nullptr; }

  void NotifyOverlayPromotion(bool promotion,
                              const gfx::Rect& bounds) override {
    stream_image_->NotifyOverlayPromotion(promotion, bounds);
  }

 private:
  scoped_refptr<StreamTextureSharedImageInterface> stream_image_;

  DISALLOW_COPY_AND_ASSIGN(SharedImageRepresentationOverlayVideo);
};

std::unique_ptr<gpu::SharedImageRepresentationOverlay>
SharedImageVideo::ProduceOverlay(gpu::SharedImageManager* manager,
                                 gpu::MemoryTypeTracker* tracker) {
  return std::make_unique<SharedImageRepresentationOverlayVideo>(manager, this,
                                                                 tracker);
}

}  // namespace gpu
