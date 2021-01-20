// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_IMAGE_READER_GL_OWNER_H_
#define GPU_COMMAND_BUFFER_SERVICE_IMAGE_READER_GL_OWNER_H_

#include <memory>

#include "base/android/android_image_reader_compat.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/service/texture_owner.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gl/gl_fence_egl.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"

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
class GPU_GLES2_EXPORT ImageReaderGLOwner : public TextureOwner {
 public:
  gl::GLContext* GetContext() const override;
  gl::GLSurface* GetSurface() const override;
  void SetFrameAvailableCallback(
      const base::RepeatingClosure& frame_available_cb) override;
  gl::ScopedJavaSurface CreateJavaSurface() const override;
  void UpdateTexImage() override;
  void EnsureTexImageBound() override;
  void ReleaseBackBuffers() override;
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBuffer() override;
  bool GetCodedSizeAndVisibleRect(gfx::Size rotated_visible_size,
                                  gfx::Size* coded_size,
                                  gfx::Rect* visible_rect) override;
  void RunWhenBufferIsAvailable(base::OnceClosure callback) override;

  const AImageReader* image_reader_for_testing() const { return image_reader_; }
  int32_t max_images_for_testing() const { return max_images_; }

 protected:
  void OnTextureDestroyed(gles2::AbstractTexture*) override;

 private:
  friend class TextureOwner;
  class ScopedHardwareBufferImpl;

  // Manages ownership of the latest image retrieved from AImageReader and
  // ensuring synchronization of its use in GL using fences.
  class ScopedCurrentImageRef {
   public:
    ScopedCurrentImageRef(ImageReaderGLOwner* texture_owner,
                          AImage* image,
                          base::ScopedFD ready_fence);
    ~ScopedCurrentImageRef();
    AImage* image() const { return image_; }
    base::ScopedFD GetReadyFence() const;
    void EnsureBound();

   private:
    ImageReaderGLOwner* texture_owner_;
    AImage* image_;
    base::ScopedFD ready_fence_;

    // Set to true if the current image is bound to |texture_id_|.
    bool image_bound_ = false;

    DISALLOW_COPY_AND_ASSIGN(ScopedCurrentImageRef);
  };

  ImageReaderGLOwner(std::unique_ptr<gles2::AbstractTexture> texture,
                     Mode secure_mode);
  ~ImageReaderGLOwner() override;

  // Registers and releases a ref on the image. Once the ref-count for an image
  // goes to 0, it is released back to the AImageReader with an optional release
  // fence if needed.
  void RegisterRefOnImage(AImage* image);
  void ReleaseRefOnImage(AImage* image, base::ScopedFD fence_fd);

  gfx::Rect GetCropRect();

  static void OnFrameAvailable(void* context, AImageReader* reader);

  // AImageReader instance
  AImageReader* image_reader_;

  // Most recently acquired image using image reader. This works like a cached
  // image until next new image is acquired which overwrites this.
  base::Optional<ScopedCurrentImageRef> current_image_ref_;
  std::unique_ptr<AImageReader_ImageListener> listener_;

  // A map consisting of pending refs on an AImage. If an image has any refs, it
  // is automatically released once the ref-count is 0.
  struct ImageRef {
    ImageRef();
    ~ImageRef();

    ImageRef(ImageRef&& other);
    ImageRef& operator=(ImageRef&& other);

    size_t count = 0u;
    base::ScopedFD release_fence_fd;

    DISALLOW_COPY_AND_ASSIGN(ImageRef);
  };
  using AImageRefMap = base::flat_map<AImage*, ImageRef>;
  AImageRefMap image_refs_;

  // reference to the class instance which is used to dynamically
  // load the functions in android libraries at runtime.
  base::android::AndroidImageReader& loader_;

  // The context and surface that were used to create |texture_id_|.
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLSurface> surface_;
  int32_t max_images_ = 0;

  // Frame available callback handling. ImageListener registered with
  // AImageReader is notified when there is a new frame available which
  // in turns runs the callback function.
  base::RepeatingClosure frame_available_cb_;

  // Runs when free buffer is available.
  base::OnceClosure buffer_available_cb_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<ImageReaderGLOwner> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ImageReaderGLOwner);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_IMAGE_READER_GL_OWNER_H_
