// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/video_image_reader_image_backing.h"

#include <utility>

#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/abstract_texture_android.h"
#include "gpu/command_buffer/service/ahardwarebuffer_utils.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/skia_gl_image_representation.h"
#include "gpu/command_buffer/service/shared_image/skia_vk_android_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/command_buffer/service/texture_owner.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_util.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "ui/gl/android/egl_fence_utils.h"
#include "ui/gl/gl_utils.h"

namespace gpu {

namespace {
void CreateAndBindEglImageFromAHB(AHardwareBuffer* buffer, GLuint service_id) {
  DCHECK(buffer);

  AHardwareBuffer_Desc desc;

  base::AndroidHardwareBufferCompat::GetInstance().Describe(buffer, &desc);
  auto egl_image = CreateEGLImageFromAHardwareBuffer(buffer);
  if (egl_image.is_valid()) {
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, service_id);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, egl_image.get());
  } else {
    LOG(ERROR) << "Failed to create EGL image ";
  }
}

class VideoImage : public base::RefCounted<VideoImage> {
 public:
  VideoImage() = default;

  VideoImage(AHardwareBuffer* buffer)
      : handle_(base::android::ScopedHardwareBufferHandle::Create(buffer)) {}

  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBuffer() {
    if (!handle_.is_valid())
      return nullptr;

    return std::make_unique<ScopedHardwareBufferFenceSyncImpl>(
        this, base::android::ScopedHardwareBufferHandle::Create(handle_.get()));
  }

  base::ScopedFD TakeEndReadFence() { return std::move(end_read_fence_); }

 protected:
 private:
  friend class base::RefCounted<VideoImage>;

  class ScopedHardwareBufferFenceSyncImpl
      : public base::android::ScopedHardwareBufferFenceSync {
   public:
    ScopedHardwareBufferFenceSyncImpl(
        scoped_refptr<VideoImage> image,
        base::android::ScopedHardwareBufferHandle handle)
        : ScopedHardwareBufferFenceSync(std::move(handle),
                                        base::ScopedFD(),
                                        base::ScopedFD(),
                                        /*is_video=*/true),
          image_(std::move(image)) {}
    ~ScopedHardwareBufferFenceSyncImpl() override = default;

    void SetReadFence(base::ScopedFD fence_fd) override {
      image_->end_read_fence_ =
          gl::MergeFDs(std::move(image_->end_read_fence_), std::move(fence_fd));
    }

   private:
    scoped_refptr<VideoImage> image_;
  };

  ~VideoImage() = default;

  base::android::ScopedHardwareBufferHandle handle_;

  // This fence should be waited upon to ensure that the reader is finished
  // reading from the buffer.
  base::ScopedFD end_read_fence_;
};

}  // namespace

VideoImageReaderImageBacking::VideoImageReaderImageBacking(
    const Mailbox& mailbox,
    const gfx::Size& size,
    const gfx::ColorSpace color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    scoped_refptr<StreamTextureSharedImageInterface> stream_texture_sii,
    scoped_refptr<SharedContextState> context_state,
    scoped_refptr<RefCountedLock> drdc_lock)
    : AndroidVideoImageBacking(mailbox,
                               size,
                               color_space,
                               surface_origin,
                               alpha_type,
                               !!drdc_lock),
      RefCountedLockHelperDrDc(std::move(drdc_lock)),
      stream_texture_sii_(std::move(stream_texture_sii)),
      gpu_main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  DCHECK(stream_texture_sii_);

  context_lost_helper_ = std::make_unique<ContextLostObserverHelper>(
      std::move(context_state), stream_texture_sii_, gpu_main_task_runner_,
      GetDrDcLock());
}

VideoImageReaderImageBacking::~VideoImageReaderImageBacking() {
  // This backing is created on gpu main thread but can be destroyed on DrDc
  // thread if the last representation was on DrDc thread.
  // |context_lost_helper_| is destroyed here by posting task to the
  // |gpu_main_thread_| to ensure that resources are cleaned up correvtly on
  // the gpu main thread.
  if (!gpu_main_task_runner_->RunsTasksInCurrentSequence()) {
    auto helper_destruction_cb = base::BindPostTask(
        gpu_main_task_runner_,
        base::BindOnce(
            [](std::unique_ptr<ContextLostObserverHelper> context_lost_helper,
               scoped_refptr<StreamTextureSharedImageInterface>
                   stream_texture_sii) {
              // Reset the |stream_texture_sii| first so that its ref in the
              // |context_lost_helper| gets reset under the DrDc lock.
              stream_texture_sii.reset();
              context_lost_helper.reset();
            }));
    std::move(helper_destruction_cb)
        .Run(std::move(context_lost_helper_), std::move(stream_texture_sii_));
  }
}

size_t VideoImageReaderImageBacking::GetEstimatedSizeForMemoryDump() const {
  base::AutoLockMaybe auto_lock(GetDrDcLockPtr());

  // This backing contributes to gpu memory only if its bound to the texture
  // and not when the backing is created.
  return stream_texture_sii_->IsUsingGpuMemory() ? GetEstimatedSize() : 0;
}

// Representation of VideoImageReaderImageBacking as a GL Texture.
class VideoImageReaderImageBacking::GLTextureVideoImageRepresentation
    : public GLTextureImageRepresentation,
      public RefCountedLockHelperDrDc {
 public:
  GLTextureVideoImageRepresentation(
      SharedImageManager* manager,
      VideoImageReaderImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<AbstractTextureAndroid> texture,
      scoped_refptr<RefCountedLock> drdc_lock)
      : GLTextureImageRepresentation(manager, backing, tracker),
        RefCountedLockHelperDrDc(std::move(drdc_lock)),
        texture_(std::move(texture)) {}

  ~GLTextureVideoImageRepresentation() override {
    if (!has_context()) {
      texture_->NotifyOnContextLost();
    }
  }

  // Disallow copy and assign.
  GLTextureVideoImageRepresentation(const GLTextureVideoImageRepresentation&) =
      delete;
  GLTextureVideoImageRepresentation& operator=(
      const GLTextureVideoImageRepresentation&) = delete;

  gles2::Texture* GetTexture(int plane_index) override {
    DCHECK_EQ(plane_index, 0);

    auto* texture = gles2::Texture::CheckedCast(texture_->GetTextureBase());
    DCHECK(texture);

    return texture;
  }

  bool BeginAccess(GLenum mode) override {
    // This representation should only be called for read.
    DCHECK(mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);

    auto* video_backing = static_cast<VideoImageReaderImageBacking*>(backing());
    {
      base::AutoLockMaybe auto_lock(GetDrDcLockPtr());
      scoped_hardware_buffer_ =
          video_backing->stream_texture_sii_->GetAHardwareBuffer();
    }
    if (!scoped_hardware_buffer_) {
      LOG(ERROR) << "Failed to get the hardware buffer.";
      return false;
    }
    CreateAndBindEglImageFromAHB(scoped_hardware_buffer_->buffer(),
                                 texture_->service_id());
    return true;
  }

  void EndAccess() override {
    DCHECK(scoped_hardware_buffer_);

    base::ScopedFD sync_fd = gl::CreateEglFenceAndExportFd();
    scoped_hardware_buffer_->SetReadFence(std::move(sync_fd));
    base::AutoLockMaybe auto_lock(GetDrDcLockPtr());
    scoped_hardware_buffer_ = nullptr;
  }

 private:
  std::unique_ptr<AbstractTextureAndroid> texture_;
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
      scoped_hardware_buffer_;
};

// Representation of VideoImageReaderImageBacking as a GL Texture.
class VideoImageReaderImageBacking::GLTexturePassthroughVideoImageRepresentation
    : public GLTexturePassthroughImageRepresentation,
      public RefCountedLockHelperDrDc {
 public:
  GLTexturePassthroughVideoImageRepresentation(
      SharedImageManager* manager,
      VideoImageReaderImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<AbstractTextureAndroid> abstract_texture,
      scoped_refptr<RefCountedLock> drdc_lock)
      : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
        RefCountedLockHelperDrDc(std::move(drdc_lock)),
        abstract_texture_(std::move(abstract_texture)),
        passthrough_texture_(gles2::TexturePassthrough::CheckedCast(
            abstract_texture_->GetTextureBase())) {
    // TODO(https://crbug.com/1172769): Remove this CHECK.
    CHECK(passthrough_texture_);
  }

  ~GLTexturePassthroughVideoImageRepresentation() override {
    if (!has_context()) {
      abstract_texture_->NotifyOnContextLost();
    }
  }

  // Disallow copy and assign.
  GLTexturePassthroughVideoImageRepresentation(
      const GLTexturePassthroughVideoImageRepresentation&) = delete;
  GLTexturePassthroughVideoImageRepresentation& operator=(
      const GLTexturePassthroughVideoImageRepresentation&) = delete;

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough(
      int plane_index) override {
    DCHECK_EQ(plane_index, 0);
    return passthrough_texture_;
  }

  bool BeginAccess(GLenum mode) override {
    // This representation should only be called for read.
    DCHECK(mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);

    auto* video_backing = static_cast<VideoImageReaderImageBacking*>(backing());

    {
      base::AutoLockMaybe auto_lock(GetDrDcLockPtr());
      scoped_hardware_buffer_ =
          video_backing->stream_texture_sii_->GetAHardwareBuffer();
    }
    if (!scoped_hardware_buffer_) {
      LOG(ERROR) << "Failed to get the hardware buffer.";
      return false;
    }
    CreateAndBindEglImageFromAHB(scoped_hardware_buffer_->buffer(),
                                 passthrough_texture_->service_id());
    return true;
  }

  void EndAccess() override {
    DCHECK(scoped_hardware_buffer_);

    base::ScopedFD sync_fd = gl::CreateEglFenceAndExportFd();
    scoped_hardware_buffer_->SetReadFence(std::move(sync_fd));
    base::AutoLockMaybe auto_lock(GetDrDcLockPtr());
    scoped_hardware_buffer_ = nullptr;
  }

 private:
  std::unique_ptr<AbstractTextureAndroid> abstract_texture_;
  scoped_refptr<gles2::TexturePassthrough> passthrough_texture_;
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
      scoped_hardware_buffer_;
};

class VideoImageReaderImageBacking::SkiaVkVideoImageRepresentation
    : public SkiaVkAndroidImageRepresentation,
      public RefCountedLockHelperDrDc {
 public:
  SkiaVkVideoImageRepresentation(
      SharedImageManager* manager,
      AndroidImageBacking* backing,
      scoped_refptr<SharedContextState> context_state,
      MemoryTypeTracker* tracker,
      scoped_refptr<RefCountedLock> drdc_lock)
      : SkiaVkAndroidImageRepresentation(manager,
                                         backing,
                                         std::move(context_state),
                                         tracker),
        RefCountedLockHelperDrDc(std::move(drdc_lock)) {}

  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    // Writes are not intended to used for video backed representations.
    NOTIMPLEMENTED();
    return {};
  }

  void EndWriteAccess() override { NOTIMPLEMENTED(); }

  std::vector<sk_sp<GrPromiseImageTexture>> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    base::AutoLockMaybe auto_lock(GetDrDcLockPtr());

    DCHECK(!scoped_hardware_buffer_);
    auto* video_backing = static_cast<VideoImageReaderImageBacking*>(backing());
    DCHECK(video_backing);
    auto* stream_texture_sii = video_backing->stream_texture_sii_.get();

    // GetAHardwareBuffer() renders the latest image and gets AHardwareBuffer
    // from it.
    scoped_hardware_buffer_ = stream_texture_sii->GetAHardwareBuffer();
    if (!scoped_hardware_buffer_) {
      LOG(ERROR) << "Failed to get the hardware buffer.";
      return {};
    }
    DCHECK(scoped_hardware_buffer_->buffer());

    // Wait on the sync fd attached to the buffer to make sure buffer is
    // ready before the read. This is done by inserting the sync fd semaphore
    // into begin_semaphore vector which client will wait on.
    init_read_fence_ = scoped_hardware_buffer_->TakeFence();

    if (!vulkan_image_) {
      DCHECK(!promise_texture_);

      vulkan_image_ = CreateVkImageFromAhbHandle(
          scoped_hardware_buffer_->TakeBuffer(), context_state(), size(),
          format(), VK_QUEUE_FAMILY_FOREIGN_EXT);
      if (!vulkan_image_)
        return {};

      // We always use VK_IMAGE_TILING_OPTIMAL while creating the vk image in
      // VulkanImplementationAndroid::CreateVkImageAndImportAHB. Hence pass
      // the tiling parameter as VK_IMAGE_TILING_OPTIMAL to below call rather
      // than passing |vk_image_info.tiling|. This is also to ensure that the
      // promise image created here at [1] as well the fulfill image created
      // via the current function call are consistent and both are using
      // VK_IMAGE_TILING_OPTIMAL. [1] -
      // https://cs.chromium.org/chromium/src/components/viz/service/display_embedder/skia_output_surface_impl.cc?rcl=db5ffd448ba5d66d9d3c5c099754e5067c752465&l=789.
      DCHECK_EQ(static_cast<int32_t>(vulkan_image_->image_tiling()),
                static_cast<int32_t>(VK_IMAGE_TILING_OPTIMAL));

      // TODO(bsalomon): Determine whether it makes sense to attempt to reuse
      // this if the vk_info stays the same on subsequent calls.
      promise_texture_ = GrPromiseImageTexture::Make(
          GrBackendTexture(size().width(), size().height(),
                           CreateGrVkImageInfo(vulkan_image_.get())));
      DCHECK(promise_texture_);
    }

    return SkiaVkAndroidImageRepresentation::BeginReadAccess(
        begin_semaphores, end_semaphores, end_state);
  }

  void EndReadAccess() override {
    base::AutoLockMaybe auto_lock(GetDrDcLockPtr());
    DCHECK(scoped_hardware_buffer_);

    SkiaVkAndroidImageRepresentation::EndReadAccess();

    // Pass the end read access sync fd to the scoped hardware buffer. This
    // will make sure that the AImage associated with the hardware buffer will
    // be deleted only when the read access is ending.
    scoped_hardware_buffer_->SetReadFence(android_backing()->TakeReadFence());
    scoped_hardware_buffer_ = nullptr;
  }

 private:
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
      scoped_hardware_buffer_;
};

std::unique_ptr<GLTextureImageRepresentation>
VideoImageReaderImageBacking::ProduceGLTexture(SharedImageManager* manager,
                                               MemoryTypeTracker* tracker) {
  base::AutoLockMaybe auto_lock(GetDrDcLockPtr());

  // For (old) overlays, we don't have a texture owner, but overlay promotion
  // might not happen for some reasons. In that case, it will try to draw
  // which should result in no image.
  if (!stream_texture_sii_->HasTextureOwner())
    return nullptr;

  // Generate an abstract texture.
  auto texture = GenAbstractTexture(/*passthrough=*/false);
  if (!texture)
    return nullptr;

  return std::make_unique<GLTextureVideoImageRepresentation>(
      manager, this, tracker, std::move(texture), GetDrDcLock());
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
VideoImageReaderImageBacking::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  base::AutoLockMaybe auto_lock(GetDrDcLockPtr());

  // For (old) overlays, we don't have a texture owner, but overlay promotion
  // might not happen for some reasons. In that case, it will try to draw
  // which should result in no image.
  if (!stream_texture_sii_->HasTextureOwner())
    return nullptr;

  // Generate an abstract texture.
  auto texture = GenAbstractTexture(/*passthrough=*/true);
  if (!texture)
    return nullptr;

  return std::make_unique<GLTexturePassthroughVideoImageRepresentation>(
      manager, this, tracker, std::move(texture), GetDrDcLock());
}

std::unique_ptr<SkiaGaneshImageRepresentation>
VideoImageReaderImageBacking::ProduceSkiaGanesh(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  base::AutoLockMaybe auto_lock(GetDrDcLockPtr());

  DCHECK(context_state);

  // For (old) overlays, we don't have a texture owner, but overlay promotion
  // might not happen for some reasons. In that case, it will try to draw
  // which should result in no image.
  if (!stream_texture_sii_->HasTextureOwner())
    return nullptr;

  if (context_state->GrContextIsVulkan()) {
    return std::make_unique<SkiaVkVideoImageRepresentation>(
        manager, this, std::move(context_state), tracker, GetDrDcLock());
  }

  DCHECK(context_state->GrContextIsGL());
  auto* texture_base = stream_texture_sii_->GetTextureBase();
  DCHECK(texture_base);
  const bool passthrough =
      (texture_base->GetType() == gpu::TextureBase::Type::kPassthrough);

  auto texture = GenAbstractTexture(passthrough);
  if (!texture)
    return nullptr;

  std::unique_ptr<gpu::GLTextureImageRepresentationBase> gl_representation;
  if (passthrough) {
    gl_representation =
        std::make_unique<GLTexturePassthroughVideoImageRepresentation>(
            manager, this, tracker, std::move(texture), GetDrDcLock());
  } else {
    gl_representation = std::make_unique<GLTextureVideoImageRepresentation>(
        manager, this, tracker, std::move(texture), GetDrDcLock());
  }
  return SkiaGLImageRepresentation::Create(std::move(gl_representation),
                                           std::move(context_state), manager,
                                           this, tracker);
}

// Representation of VideoImageReaderImageBacking as an overlay plane.
class VideoImageReaderImageBacking::OverlayVideoImageRepresentation
    : public gpu::OverlayImageRepresentation,
      public RefCountedLockHelperDrDc {
 public:
  OverlayVideoImageRepresentation(gpu::SharedImageManager* manager,
                                  VideoImageReaderImageBacking* backing,
                                  gpu::MemoryTypeTracker* tracker,
                                  scoped_refptr<RefCountedLock> drdc_lock)
      : gpu::OverlayImageRepresentation(manager, backing, tracker),
        RefCountedLockHelperDrDc(std::move(drdc_lock)) {}

  // Disallow copy and assign.
  OverlayVideoImageRepresentation(const OverlayVideoImageRepresentation&) =
      delete;
  OverlayVideoImageRepresentation& operator=(
      const OverlayVideoImageRepresentation&) = delete;

 protected:
  bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) override {
    base::AutoLockMaybe auto_lock(GetDrDcLockPtr());
    // A |CodecImage| must have TextureOwner() for SurfaceControl overlays.
    // Legacy overlays are handled by LegacyOverlayImageRepresentation.
    // Unfortunately it's possible that underlying CodecImage has released its
    // resources due to MediaCodec shutdown, so we don't DCHECK here.

    scoped_hardware_buffer_ = stream_image()->GetAHardwareBuffer();

    // |scoped_hardware_buffer_| could be null for cases when a buffer is
    // not acquired in ImageReader for some reasons and there is no previously
    // acquired image left.
    if (!scoped_hardware_buffer_)
      return false;

    gfx::GpuFenceHandle handle;
    handle.owned_fd = scoped_hardware_buffer_->TakeFence();
    if (!handle.is_null())
      acquire_fence = std::move(handle);

    return true;
  }

  void EndReadAccess(gfx::GpuFenceHandle release_fence) override {
    if (video_image_) {
      DCHECK(release_fence.is_null());
      if (scoped_hardware_buffer_) {
        scoped_hardware_buffer_->SetReadFence(video_image_->TakeEndReadFence());
      }
      video_image_.reset();
    } else {
      scoped_hardware_buffer_->SetReadFence(std::move(release_fence.owned_fd));
    }

    base::AutoLockMaybe auto_lock(GetDrDcLockPtr());
    scoped_hardware_buffer_.reset();
  }

  AHardwareBuffer* GetAHardwareBuffer() override {
    DCHECK(scoped_hardware_buffer_);
    return scoped_hardware_buffer_->buffer();
  }

  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBufferFenceSync() override {
    return GetVideoImage()->GetAHardwareBuffer();
  }

 private:
  VideoImage* GetVideoImage() {
    base::AutoLockMaybe auto_lock(GetDrDcLockPtr());
    DCHECK(scoped_hardware_buffer_);

    if (!video_image_) {
      video_image_ =
          base::MakeRefCounted<VideoImage>(scoped_hardware_buffer_->buffer());
    }
    return video_image_.get();
  }

  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
      scoped_hardware_buffer_;
  scoped_refptr<VideoImage> video_image_;

  StreamTextureSharedImageInterface* stream_image() {
    auto* video_backing = static_cast<VideoImageReaderImageBacking*>(backing());
    DCHECK(video_backing);
    return video_backing->stream_texture_sii_.get();
  }
};

// Representation of VideoImageReaderImageBacking as an SurfaceView overlay
// plane.
class VideoImageReaderImageBacking::LegacyOverlayVideoImageRepresentation
    : public gpu::LegacyOverlayImageRepresentation,
      public RefCountedLockHelperDrDc {
 public:
  LegacyOverlayVideoImageRepresentation(gpu::SharedImageManager* manager,
                                        VideoImageReaderImageBacking* backing,
                                        gpu::MemoryTypeTracker* tracker,
                                        scoped_refptr<RefCountedLock> drdc_lock)
      : gpu::LegacyOverlayImageRepresentation(manager, backing, tracker),
        RefCountedLockHelperDrDc(std::move(drdc_lock)) {}

  void RenderToOverlay() override {
    TRACE_EVENT0("media",
                 "LegacyOverlayVideoImageRepresentation::RenderToOverlay");

    base::AutoLockMaybe auto_lock(GetDrDcLockPtr());
    auto* stream_texture_sii = stream_image();
    DCHECK(!stream_texture_sii->HasTextureOwner())
        << "Image must be promoted to overlay first.";
    stream_texture_sii->RenderToOverlay();
  }

  void NotifyOverlayPromotion(bool promotion,
                              const gfx::Rect& bounds) override {
    base::AutoLockMaybe auto_lock(GetDrDcLockPtr());
    stream_image()->NotifyOverlayPromotion(promotion, bounds);
  }

  StreamTextureSharedImageInterface* stream_image() {
    auto* video_backing = static_cast<VideoImageReaderImageBacking*>(backing());
    DCHECK(video_backing);
    return video_backing->stream_texture_sii_.get();
  }
};

std::unique_ptr<gpu::OverlayImageRepresentation>
VideoImageReaderImageBacking::ProduceOverlay(gpu::SharedImageManager* manager,
                                             gpu::MemoryTypeTracker* tracker) {
  return std::make_unique<OverlayVideoImageRepresentation>(
      manager, this, tracker, GetDrDcLock());
}

std::unique_ptr<gpu::LegacyOverlayImageRepresentation>
VideoImageReaderImageBacking::ProduceLegacyOverlay(
    gpu::SharedImageManager* manager,
    gpu::MemoryTypeTracker* tracker) {
  return std::make_unique<LegacyOverlayVideoImageRepresentation>(
      manager, this, tracker, GetDrDcLock());
}

VideoImageReaderImageBacking::ContextLostObserverHelper::
    ContextLostObserverHelper(
        scoped_refptr<SharedContextState> context_state,
        scoped_refptr<StreamTextureSharedImageInterface> stream_texture_sii,
        scoped_refptr<base::SingleThreadTaskRunner> gpu_main_task_runner,
        scoped_refptr<RefCountedLock> drdc_lock)
    : RefCountedLockHelperDrDc(std::move(drdc_lock)),
      context_state_(std::move(context_state)),
      stream_texture_sii_(std::move(stream_texture_sii)),
      gpu_main_task_runner_(std::move(gpu_main_task_runner)) {
  DCHECK(context_state_);
  DCHECK(stream_texture_sii_);

  context_state_->AddContextLostObserver(this);
}

VideoImageReaderImageBacking::ContextLostObserverHelper::
    ~ContextLostObserverHelper() {
  DCHECK(gpu_main_task_runner_->RunsTasksInCurrentSequence());

  if (context_state_)
    context_state_->RemoveContextLostObserver(this);
  {
    base::AutoLockMaybe auto_lock(GetDrDcLockPtr());
    stream_texture_sii_->ReleaseResources();
    stream_texture_sii_.reset();
  }
}

// SharedContextState::ContextLostObserver implementation.
void VideoImageReaderImageBacking::ContextLostObserverHelper::OnContextLost() {
  DCHECK(gpu_main_task_runner_->RunsTasksInCurrentSequence());
  base::AutoLockMaybe auto_lock(GetDrDcLockPtr());

  // We release codec buffers when shared image context is lost. This is
  // because texture owner's texture was created on shared context. Once
  // shared context is lost, no one should try to use that texture.
  stream_texture_sii_->ReleaseResources();
  context_state_->RemoveContextLostObserver(this);
  context_state_ = nullptr;
}

}  // namespace gpu
