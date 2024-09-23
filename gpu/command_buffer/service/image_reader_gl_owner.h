// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_IMAGE_READER_GL_OWNER_H_
#define GPU_COMMAND_BUFFER_SERVICE_IMAGE_READER_GL_OWNER_H_

#include <media/NdkImageReader.h>

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "gpu/command_buffer/service/texture_owner.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gl/gl_fence_egl.h"

namespace base {
namespace android {
class ScopedHardwareBufferFenceSync;
}  // namespace android
}  // namespace base

namespace gpu {

// This class wraps the AImageReader usage and is used to create a GL texture
// using the current platform GL context and returns a new ImageReaderGLOwner
// attached to it. The surface handle of the AImageReader is attached to
// decoded media frames. Media frames can update the attached surface handle
// with image data and this class helps to create an eglImage using that image
// data present in the surface.
class GPU_GLES2_EXPORT ImageReaderGLOwner : public TextureOwner,
                                            public RefCountedLockHelperDrDc {
 public:
  ImageReaderGLOwner(const ImageReaderGLOwner&) = delete;
  ImageReaderGLOwner& operator=(const ImageReaderGLOwner&) = delete;

  gl::GLContext* GetContext() const override;
  gl::GLSurface* GetSurface() const override;
  void SetFrameAvailableCallback(
      const base::RepeatingClosure& frame_available_cb) override;
  gl::ScopedJavaSurface CreateJavaSurface() const override;
  void UpdateTexImage() override;
  void ReleaseBackBuffers() override;
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBuffer() override;
  bool GetCodedSizeAndVisibleRect(gfx::Size rotated_visible_size,
                                  gfx::Size* coded_size,
                                  gfx::Rect* visible_rect) override;

  void RunWhenBufferIsAvailable(base::OnceClosure callback) override;

  const AImageReader* image_reader_for_testing() const
      NO_THREAD_SAFETY_ANALYSIS {
    return image_reader_;
  }
  int32_t max_images_for_testing() const { return max_images_; }

  // MemoryDumpProvider:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 protected:
  void ReleaseResources() override;

 private:
  friend class TextureOwner;
  friend class ImageReaderGLOwnerTest;
  class ScopedHardwareBufferImpl;

  // Manages ownership of the latest image retrieved from AImageReader and
  // ensuring synchronization of its use in GL using fences.
  class ScopedCurrentImageRef {
   public:
    ScopedCurrentImageRef(ImageReaderGLOwner* texture_owner,
                          AImage* image,
                          base::ScopedFD ready_fence);

    ScopedCurrentImageRef(const ScopedCurrentImageRef&) = delete;
    ScopedCurrentImageRef& operator=(const ScopedCurrentImageRef&) = delete;

    ~ScopedCurrentImageRef();
    AImage* image() const { return image_; }
    base::ScopedFD GetReadyFence() const;

   private:
    raw_ptr<ImageReaderGLOwner> texture_owner_;
    raw_ptr<AImage> image_;
    base::ScopedFD ready_fence_;
  };

  ImageReaderGLOwner(std::unique_ptr<AbstractTextureAndroid> texture,
                     Mode secure_mode,
                     scoped_refptr<SharedContextState> context_state,
                     scoped_refptr<RefCountedLock> drdc_lock,
                     TextureOwnerCodecType type_for_metrics);
  ~ImageReaderGLOwner() override;

  // Registers and releases a ref on the image. Once the ref-count for an image
  // goes to 0, it is released back to the AImageReader with an optional release
  // fence if needed.
  void RegisterRefOnImageLocked(AImage* image);
  void ReleaseRefOnImageLocked(AImage* image, base::ScopedFD fence_fd);

  // This method acquires |lock_| and calls ReleaseRefOnImageLocked().
  void ReleaseRefOnImage(AImage* image, base::ScopedFD fence_fd);

  gfx::Rect GetCropRectLocked();

  static void OnFrameAvailable(void* context, AImageReader* reader);

  // All members which can be concurrently accessed from multiple threads will
  // be guarded by |lock_|.
  mutable base::Lock lock_;

  // AImageReader instance.
  raw_ptr<AImageReader> image_reader_ GUARDED_BY(lock_);

  // Most recently acquired image using image reader. This works like a cached
  // image until next new image is acquired which overwrites this.
  std::optional<ScopedCurrentImageRef> current_image_ref_ GUARDED_BY(lock_);
  std::unique_ptr<AImageReader_ImageListener> listener_;

  // A map consisting of pending refs on an AImage. If an image has any refs, it
  // is automatically released once the ref-count is 0.
  struct ImageRef {
    ImageRef();

    ImageRef(const ImageRef&) = delete;
    ImageRef& operator=(const ImageRef&) = delete;

    ~ImageRef();

    ImageRef(ImageRef&& other);
    ImageRef& operator=(ImageRef&& other);

    size_t count = 0u;
    base::ScopedFD release_fence_fd;
    gfx::Size size;
    size_t estimated_size_in_bytes = 0;
  };
  using AImageRefMap = base::flat_map<AImage*, ImageRef>;
  AImageRefMap image_refs_ GUARDED_BY(lock_);
  std::atomic<size_t> total_estimated_size_in_bytes_ = 0;

  // The context and surface that were used to create |texture_id_|.
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLSurface> surface_;
  int32_t max_images_ = 0;

  // Frame available callback handling. ImageListener registered with
  // AImageReader is notified when there is a new frame available which
  // in turns runs the callback function.
  base::RepeatingClosure frame_available_cb_;

  // Runs when free buffer is available.
  base::OnceClosure buffer_available_cb_ GUARDED_BY(lock_);

  // This class is created on gpu main thread.
  THREAD_CHECKER(gpu_main_thread_checker_);

  const TextureOwnerCodecType type_for_metrics_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_IMAGE_READER_GL_OWNER_H_
