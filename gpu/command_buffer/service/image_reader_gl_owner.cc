// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/image_reader_gl_owner.h"

#include <android/native_window_jni.h>
#include <jni.h>
#include <stdint.h>

#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/posix/eintr_wrapper.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "gpu/command_buffer/service/abstract_texture_android.h"
#include "gpu/config/gpu_finch_features.h"
#include "ui/gfx/android/android_surface_control_compat.h"
#include "ui/gl/gl_fence_android_native_fence_sync.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {

namespace {

BASE_FEATURE(kAlwaysUsePrivateFormatForImageReader,
             "AlwaysUsePrivateFormatForImageReader",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsSurfaceControl(TextureOwner::Mode mode) {
  switch (mode) {
    case TextureOwner::Mode::kAImageReaderInsecureSurfaceControl:
    case TextureOwner::Mode::kAImageReaderSecureSurfaceControl:
      return true;
    case TextureOwner::Mode::kAImageReaderInsecure:
      return false;
    case TextureOwner::Mode::kSurfaceTextureInsecure:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

// This should be as small as possible to limit the memory usage.
// ImageReader needs 1 image to mimic the behavior of SurfaceTexture but
// 2 images are required to minimize negative impact on
// smoothness. This is because in case an image is not acquired for some
// reasons, last acquired image should be displayed which is only possible with
// 2 images (1 previously acquired, 1 currently acquired/tried to acquire).
// But some devices supports only 1 image to be acquired. (see
// crbug.com/1051705). For SurfaceControl we need 3 images instead of 2 since 1
// frame (and hence image associated with it) will be with system compositor and
// 2 frames will be in flight. For multi-threaded compositor, when AImageReader
// is supported, we need 3 images in order to skip texture copy. 1 frame with
// display compositor, 1 frame in flight and 1 frame being prepared by the
// renderer.
uint32_t NumRequiredMaxImages(TextureOwner::Mode mode) {
  if (IsSurfaceControl(mode)) {
    DCHECK(!features::LimitAImageReaderMaxSizeToOne());
    if (features::IncreaseBufferCountForHighFrameRate())
      return 5;

    return 3;
  }
  return features::LimitAImageReaderMaxSizeToOne() ? 1 : 2;
}

std::optional<gfx::Size> GetImageSize(AImage* image) {
  int32_t width = 0, height = 0;
  if (AImage_getWidth(image, &width) != AMEDIA_OK ||
      AImage_getHeight(image, &height) != AMEDIA_OK || width <= 0 ||
      height <= 0) {
    return std::nullopt;
  }

  return gfx::Size(width, height);
}

}  // namespace

// This class is safe to be created/destroyed on different threads. This is made
// sure by destruction happening on correct thread. This class is not thread
// safe to be used concurrently on multiple thraeads.
class ImageReaderGLOwner::ScopedHardwareBufferImpl
    : public base::android::ScopedHardwareBufferFenceSync {
 public:
  ScopedHardwareBufferImpl(scoped_refptr<ImageReaderGLOwner> texture_owner,
                           AImage* image,
                           base::android::ScopedHardwareBufferHandle handle,
                           base::ScopedFD fence_fd)
      : base::android::ScopedHardwareBufferFenceSync(std::move(handle),
                                                     std::move(fence_fd),
                                                     base::ScopedFD()),
        texture_owner_(std::move(texture_owner)),
        image_(image) {
    DCHECK(image_);
    texture_owner_->RegisterRefOnImageLocked(image_);
  }

  ~ScopedHardwareBufferImpl() override {
    texture_owner_->ReleaseRefOnImage(image_, std::move(read_fence_));
  }

  void SetReadFence(base::ScopedFD fence_fd) final {
    // Client can call this method multiple times for a hardware buffer. Hence
    // all the client provided sync_fd should be merged. Eg: BeginReadAccess()
    // can be called multiple times for an AndroidVideoImageBacking
    // representation.
    read_fence_ = gl::MergeFDs(std::move(read_fence_), std::move(fence_fd));
  }

 private:
  base::ScopedFD read_fence_;
  scoped_refptr<ImageReaderGLOwner> texture_owner_;
  raw_ptr<AImage> image_;
};

ImageReaderGLOwner::ImageReaderGLOwner(
    std::unique_ptr<AbstractTextureAndroid> texture,
    Mode mode,
    scoped_refptr<SharedContextState> context_state,
    scoped_refptr<RefCountedLock> drdc_lock,
    TextureOwnerCodecType type_for_metrics)
    : TextureOwner(false /* binds_texture_on_image_update */,
                   std::move(texture),
                   std::move(context_state)),
      RefCountedLockHelperDrDc(std::move(drdc_lock)),
      context_(gl::GLContext::GetCurrent()),
      surface_(gl::GLSurface::GetCurrent()),
      type_for_metrics_(type_for_metrics) {
  DCHECK(context_);
  DCHECK(surface_);

  // Set the width, height and format to some default value. This parameters
  // are/maybe overriden by the producer sending buffers to this imageReader's
  // Surface.
  int32_t width = 1, height = 1;
  max_images_ = NumRequiredMaxImages(mode);
  AIMAGE_FORMATS format = mode == Mode::kAImageReaderSecureSurfaceControl
                              ? AIMAGE_FORMAT_PRIVATE
                              : AIMAGE_FORMAT_YUV_420_888;

  if (base::FeatureList::IsEnabled(kAlwaysUsePrivateFormatForImageReader)) {
    format = AIMAGE_FORMAT_PRIVATE;
  }

  AImageReader* reader = nullptr;

  // The usage flag below should be used when the buffer will be read from by
  // the GPU as a texture.
  uint64_t usage = mode == Mode::kAImageReaderSecureSurfaceControl
                       ? AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT
                       : AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
  if (IsSurfaceControl(mode))
    usage |= AHARDWAREBUFFER_USAGE_COMPOSER_OVERLAY;

  // Create a new reader for images of the desired size and format.
  media_status_t return_code = AImageReader_newWithUsage(
      width, height, format, usage, max_images_, &reader);
  if (return_code != AMEDIA_OK) {
    LOG(ERROR) << " Image reader creation failed on device model : "
               << base::android::BuildInfo::GetInstance()->model()
               << ". maxImages used is : " << max_images_;
    base::debug::DumpWithoutCrashing();
    if (return_code == AMEDIA_ERROR_INVALID_PARAMETER) {
      LOG(ERROR) << "Either reader is null, or one or more of width, height, "
                    "format, maxImages arguments is not supported";
    } else {
      LOG(ERROR) << "unknown error";
    }
    return;
  }
  DCHECK(reader);
  image_reader_ = reader;

  // Create a new Image Listner.
  listener_ = std::make_unique<AImageReader_ImageListener>();

  // Passing |this| is safe here since we stop listening to new images in the
  // destructor and set the ImageListener to null.
  listener_->context = reinterpret_cast<void*>(this);
  listener_->onImageAvailable = &ImageReaderGLOwner::OnFrameAvailable;

  // Set the onImageAvailable listener of this image reader.
  if (AImageReader_setImageListener(image_reader_, listener_.get()) !=
      AMEDIA_OK) {
    LOG(ERROR) << " Failed to register AImageReader listener";
    return;
  }
}

ImageReaderGLOwner::~ImageReaderGLOwner() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);

  // Call ReleaseResources() if it hasn't already. This will do nothing if the
  // texture and other resources has already been destroyed due to context loss.
  ReleaseResources();

  DCHECK_EQ(image_refs_.size(), 0u);
}

void ImageReaderGLOwner::ReleaseResources() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  base::AutoLock auto_lock(lock_);
  // Either TextureOwner is being destroyed or the TextureOwner's shared context
  // is lost. Cleanup is it hasn't already.
  if (image_reader_) {
    // Now we can stop listening to new images.
    AImageReader_setImageListener(image_reader_, nullptr);

    // Delete all images before closing the associated image reader.
    for (auto& image_ref : image_refs_)
      AImage_delete(image_ref.first);

    // Delete the image reader.
    AImageReader_delete(image_reader_);
    image_reader_ = nullptr;

    // Clean up the ImageRefs which should now be a no-op since there is no
    // valid |image_reader_|.
    image_refs_.clear();
    current_image_ref_.reset();
    total_estimated_size_in_bytes_ = 0;
  }
}

void ImageReaderGLOwner::SetFrameAvailableCallback(
    const base::RepeatingClosure& frame_available_cb) {
  DCHECK(!frame_available_cb_);
  frame_available_cb_ = std::move(frame_available_cb);
}

gl::ScopedJavaSurface ImageReaderGLOwner::CreateJavaSurface() const {
  base::AutoLock auto_lock(lock_);

  // If we've already lost the texture, then do nothing.
  if (!image_reader_) {
    DLOG(ERROR) << "Already lost texture / image reader";
    return nullptr;
  }

  // Get the android native window from the image reader.
  ANativeWindow* window = nullptr;
  if (AImageReader_getWindow(image_reader_, &window) != AMEDIA_OK) {
    DLOG(ERROR) << "unable to get a window from image reader.";
    return nullptr;
  }

  // Get the java surface object from the Android native window.
  JNIEnv* env = base::android::AttachCurrentThread();
  auto j_surface = base::android::ScopedJavaLocalRef<jobject>::Adopt(
      env, ANativeWindow_toSurface(env, window));
  DCHECK(j_surface);

  // Get the scoped java surface that will call release() on destruction.
  return gl::ScopedJavaSurface(j_surface, /*auto_release=*/true);
}

void ImageReaderGLOwner::UpdateTexImage() {
  base::AutoLock auto_lock(lock_);

  // If we've lost the texture, then do nothing.
  if (!texture())
    return;

  DCHECK(image_reader_);

  // Acquire the latest image asynchronously. We must release the current image
  // before acquiring a new one if the ImageReader was initialized with one
  // outstanding image at max.
  if (max_images_ == 1)
    current_image_ref_.reset();

  AImage* image = nullptr;
  int acquire_fence_fd = -1;
  media_status_t return_code = AMEDIA_OK;

  if (max_images_ - image_refs_.size() < 2) {
    // acquireNextImageAsync is required here since as per the spec calling
    // AImageReader_acquireLatestImage with less than two images of margin, that
    // is (maxImages - currentAcquiredImages < 2) will not discard as expected.
    // We always have currentAcquiredImages as 1 since we delete a previous
    // image only after acquiring a new image.
    return_code = AImageReader_acquireNextImageAsync(image_reader_, &image,
                                                     &acquire_fence_fd);
  } else {
    return_code = AImageReader_acquireLatestImageAsync(image_reader_, &image,
                                                       &acquire_fence_fd);
  }
  base::UmaHistogramSparse("Media.AImageReaderGLOwner.AcquireImageResult",
                           return_code);

  UMA_HISTOGRAM_ENUMERATION("Media.AImageReaderGLOwner.CodecType",
                            type_for_metrics_);

  // TODO(http://crbug.com/846050).
  // Need to add some better error handling if below error occurs. Currently we
  // just return if error occurs.
  switch (return_code) {
    case AMEDIA_ERROR_INVALID_PARAMETER:
      LOG(ERROR) << "AImageReader: Invalid parameter";
      return;
    case AMEDIA_IMGREADER_MAX_IMAGES_ACQUIRED:
      LOG(ERROR)
          << "number of concurrently acquired images has reached the limit";
      return;
    case AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE:
      LOG(ERROR) << "no buffers currently available in the reader queue";
      return;
    case AMEDIA_ERROR_UNKNOWN:
      LOG(ERROR) << "method fails for some other reasons";
      return;
    case AMEDIA_OK:
      // Method call succeeded.
      break;
    default:
      LOG(ERROR) << "AImageReader: Unknown error: " << return_code;
      // No other error code should be returned.
      NOTREACHED_IN_MIGRATION();
      return;
  }
  base::ScopedFD scoped_acquire_fence_fd(acquire_fence_fd);

  // If there is no new image simply return. At this point previous image will
  // still be bound to the texture.
  if (!image) {
    LOG(ERROR) << "AImageReader: image is nullptr: " << return_code;
    return;
  }

  UMA_HISTOGRAM_BOOLEAN("Media.AImageReaderGLOwner.HasFence",
                        scoped_acquire_fence_fd.is_valid());

  // Make the newly acquired image as current image.
  current_image_ref_.emplace(this, image, std::move(scoped_acquire_fence_fd));
}

std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
ImageReaderGLOwner::GetAHardwareBuffer() {
  base::AutoLock auto_lock(lock_);
  if (!current_image_ref_)
    return nullptr;

  AHardwareBuffer* buffer = nullptr;
  auto error = AImage_getHardwareBuffer(current_image_ref_->image(), &buffer);
  if (!buffer) {
    LOG(ERROR) << "AImage_getHardwareBuffer returned nullptr: " << error;
    return nullptr;
  }

  // TODO(crbug.com/40749597): We suspect that buffer is already freed here and
  // it causes crash later. Trying to crash earlier.
  base::AndroidHardwareBufferCompat::GetInstance().Acquire(buffer);
  base::AndroidHardwareBufferCompat::GetInstance().Release(buffer);

  return std::make_unique<ScopedHardwareBufferImpl>(
      this, current_image_ref_->image(),
      base::android::ScopedHardwareBufferHandle::Create(buffer),
      current_image_ref_->GetReadyFence());
}

gfx::Rect ImageReaderGLOwner::GetCropRectLocked() {
  lock_.AssertAcquired();
  if (!current_image_ref_)
    return gfx::Rect();

  // Note that to query the crop rectangle, we don't need to wait for the
  // AImage to be ready by checking the associated image ready fence.
  AImageCropRect crop_rect;
  media_status_t return_code =
      AImage_getCropRect(current_image_ref_->image(), &crop_rect);
  if (return_code != AMEDIA_OK) {
    DLOG(ERROR) << "Error querying crop rectangle from the image : "
                << return_code;
    return gfx::Rect();
  }
  DCHECK_GE(crop_rect.right, crop_rect.left);
  DCHECK_GE(crop_rect.bottom, crop_rect.top);
  return gfx::Rect(crop_rect.left, crop_rect.top,
                   crop_rect.right - crop_rect.left,
                   crop_rect.bottom - crop_rect.top);
}

void ImageReaderGLOwner::RegisterRefOnImageLocked(AImage* image) {
  lock_.AssertAcquired();
  DCHECK(image_reader_);

  auto& ref = image_refs_[image];
  // Add a ref that the caller will release.
  if (ref.count++ == 0) {
    if (auto size = GetImageSize(image)) {
      ref.size = size.value();

      // We don't know the exact format of the image so we use NV12 as
      // approximation as the most popular format.
      constexpr auto format = viz::MultiPlaneFormat::kNV12;
      ref.estimated_size_in_bytes = format.EstimatedSizeInBytes(ref.size);

      total_estimated_size_in_bytes_ += ref.estimated_size_in_bytes;
    }
  }
}

void ImageReaderGLOwner::ReleaseRefOnImage(AImage* image,
                                           base::ScopedFD fence_fd) {
  base::AutoLock auto_lock(lock_);
  ReleaseRefOnImageLocked(image, std::move(fence_fd));
}

void ImageReaderGLOwner::ReleaseRefOnImageLocked(AImage* image,
                                                 base::ScopedFD fence_fd) {
  lock_.AssertAcquired();

  // During cleanup on losing the texture, all images are synchronously released
  // and the |image_reader_| is destroyed.
  if (!image_reader_)
    return;

  // Ensure that DrDc lock is held when |buffer_available_cb| can be triggered
  // because we do not want any other thread to steal the free buffer slot which
  // is meant to be used by |buffer_available_cb| and hence resulting in wrong
  // FrameInfo for all future frames.
  AssertAcquiredDrDcLock();

  auto it = image_refs_.find(image);
  CHECK(it != image_refs_.end(), base::NotFatalUntil::M130);

  auto& image_ref = it->second;
  DCHECK_GT(image_ref.count, 0u);
  image_ref.count--;
  image_ref.release_fence_fd =
      gl::MergeFDs(std::move(image_ref.release_fence_fd), std::move(fence_fd));

  if (image_ref.count > 0)
    return;

  if (image_ref.release_fence_fd.is_valid()) {
    AImage_deleteAsync(image, std::move(image_ref.release_fence_fd.release()));
  } else {
    AImage_delete(image);
  }

  total_estimated_size_in_bytes_ -= it->second.estimated_size_in_bytes;

  image_refs_.erase(it);
  DCHECK_GT(max_images_, static_cast<int32_t>(image_refs_.size()));
  auto buffer_available_cb = std::move(buffer_available_cb_);

  // |buffer_available_cb| will try to acquire lock again via
  // UpdatetexImage(), hence we need to unlock here. Note that when
  // |max_images_| is 1, this callback will always be empty here since it will
  // be run immediately in RunWhenBufferIsAvailable(). Hence resetting
  // |current_image_ref_| in UpdateTexImage() can not trigger this callback.
  // Otherwise triggering this callback from UpdateTexImage() on
  // |current_image_ref_| reset would cause callback and hence FrameInfoHelper
  // to run and eventually call UpdateTexImage() from there which could have
  // been filmsy.
  if (buffer_available_cb) {
    base::AutoUnlock auto_unlock(lock_);
    DCHECK_GT(max_images_, 1);
    std::move(buffer_available_cb).Run();
  }
}

void ImageReaderGLOwner::ReleaseBackBuffers() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  // ReleaseBackBuffers() call is not required with image reader.
}

gl::GLContext* ImageReaderGLOwner::GetContext() const {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  return context_.get();
}

gl::GLSurface* ImageReaderGLOwner::GetSurface() const {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  return surface_.get();
}

// This callback function will be called when there is a new image available
// for in the image reader's queue.
void ImageReaderGLOwner::OnFrameAvailable(void* context, AImageReader* reader) {
  ImageReaderGLOwner* image_reader_ptr =
      reinterpret_cast<ImageReaderGLOwner*>(context);

  // It is safe to run this callback on any thread.
  image_reader_ptr->frame_available_cb_.Run();
}

void ImageReaderGLOwner::RunWhenBufferIsAvailable(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  int image_refs_size = 0;
  {
    base::AutoLock auto_lock(lock_);
    // Note that we handle only one simultaneous request, this is not issue
    // because FrameInfoHelper maintain request queue and has only single
    // outstanding request on GPU thread.
    DCHECK(!buffer_available_cb_);
    image_refs_size = static_cast<int>(image_refs_.size());
  }
  // If `max_images` == 1 we will drop it before acquiring new buffer. Note
  // that this must never happen with SurfaceControl and the
  // ImageReaderGLOwner is the sole owner of the images.
  if (max_images_ == 1 || image_refs_size < max_images_) {
    // This callback is run from here as well as from ReleaseRefOnImage() where
    // we remove one image from image reader queue before callback is run.
    // Once the |lock_| is dropped in this method here, another thread can
    // UpdateTexImage() before callback is run and hence cause the image reader
    // queue to become full. In that case callback will not be able to render
    // and acquire updated image and hence will use FrameInfo of the previous
    // image which will result in wrong coded size for all future frames. To
    // avoid this, no other thread should try to UpdateTexImage() when this
    // callback is run. Hence drdc_lock should be held from all the places from
    // where the callback could be run which is either OnGpu::GetFrameInfo() or
    // ImageReaderGLOwner::ReleaseRefOnImageLocked() and
    // OnGpu::GetFrameInfoImpl() should assume that the drdc_lock is always
    // held.
    std::move(callback).Run();
  } else {
    base::AutoLock auto_lock(lock_);
    buffer_available_cb_ = std::move(callback);
  }
}

bool ImageReaderGLOwner::GetCodedSizeAndVisibleRect(
    gfx::Size rotated_visible_size,
    gfx::Size* coded_size,
    gfx::Rect* visible_rect) {
  base::AutoLock auto_lock(lock_);
  DCHECK(visible_rect);
  DCHECK(coded_size);

  AHardwareBuffer* buffer = nullptr;
  if (current_image_ref_) {
    AImage_getHardwareBuffer(current_image_ref_->image(), &buffer);
    if (!buffer) {
      DLOG(ERROR) << "Unable to get an AHardwareBuffer from the image";
    }
  }
  if (!buffer) {
    *coded_size = gfx::Size();
    *visible_rect = gfx::Rect();
    return false;
  }
  // Get the buffer descriptor. Note that for querying the buffer descriptor, we
  // do not need to wait on the AHB to be ready.
  AHardwareBuffer_Desc desc;
  base::AndroidHardwareBufferCompat::GetInstance().Describe(buffer, &desc);

  *visible_rect = GetCropRectLocked();
  *coded_size = gfx::Size(desc.width, desc.height);

  return true;
}

bool ImageReaderGLOwner::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  if (args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::kBackground) {
    auto dump_name =
        base::StringPrintf("gpu/media_texture_owner_%d/", tracing_id());

    base::trace_event::MemoryAllocatorDump* dump =
        pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    total_estimated_size_in_bytes_);
    // Early out, no need for more detail in a BACKGROUND dump.
    return true;
  }

  int i = 0;
  base::AutoLock auto_lock(lock_);
  for (const auto& image : image_refs_) {
    std::string dump_name = base::StringPrintf(
        "gpu/media_texture_owner_%d/image_%d", tracing_id(), i++);

    // If we fail to get AImage size for any reason, we still report the image
    // as a empty size, so it can be diagnosed in necessary.
    base::trace_event::MemoryAllocatorDump* dump =
        pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    image.second.estimated_size_in_bytes);
    dump->AddString("dimensions", "", image.second.size.ToString());
  }

  return true;
}

ImageReaderGLOwner::ImageRef::ImageRef() = default;
ImageReaderGLOwner::ImageRef::~ImageRef() = default;
ImageReaderGLOwner::ImageRef::ImageRef(ImageRef&& other) = default;
ImageReaderGLOwner::ImageRef& ImageReaderGLOwner::ImageRef::operator=(
    ImageRef&& other) = default;

ImageReaderGLOwner::ScopedCurrentImageRef::ScopedCurrentImageRef(
    ImageReaderGLOwner* texture_owner,
    AImage* image,
    base::ScopedFD ready_fence)
    : texture_owner_(texture_owner),
      image_(image),
      ready_fence_(std::move(ready_fence)) {
  DCHECK(texture_owner_);
  texture_owner_->lock_.AssertAcquired();
  DCHECK(image_);
  texture_owner_->RegisterRefOnImageLocked(image_);
}

ImageReaderGLOwner::ScopedCurrentImageRef::~ScopedCurrentImageRef() {
  texture_owner_->lock_.AssertAcquired();
  texture_owner_->ReleaseRefOnImageLocked(image_, std::move(ready_fence_));
}

base::ScopedFD ImageReaderGLOwner::ScopedCurrentImageRef::GetReadyFence()
    const {
  return base::ScopedFD(HANDLE_EINTR(dup(ready_fence_.get())));
}

}  // namespace gpu
