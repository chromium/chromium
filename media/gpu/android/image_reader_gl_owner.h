// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_IMAGE_READER_GL_OWNER_H_
#define MEDIA_GPU_ANDROID_IMAGE_READER_GL_OWNER_H_

#include <memory>

#include "base/android/android_image_reader_compat.h"
#include "base/containers/flat_map.h"
#include "media/gpu/android/texture_owner.h"
#include "ui/gl/gl_fence_egl.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"

namespace media {

struct FrameAvailableEvent_ImageReader;

// This class wraps the AImageReader usage and is used to create a GL texture
// using the current platform GL context and returns a new ImageReaderGLOwner
// attached to it. The surface handle of the AImageReader is attached to
// decoded media frames. Media frames can update the attached surface handle
// with image data and this class helps to create an eglImage using that image
// data present in the surface.
class MEDIA_GPU_EXPORT ImageReaderGLOwner : public TextureOwner {
 public:
  GLuint GetTextureId() const override;
  gl::GLContext* GetContext() const override;
  gl::GLSurface* GetSurface() const override;
  gl::ScopedJavaSurface CreateJavaSurface() const override;
  void UpdateTexImage() override;
  void GetTransformMatrix(float mtx[16]) override;
  void ReleaseBackBuffers() override;
  void SetReleaseTimeToNow() override;
  void IgnorePendingRelease() override;
  bool IsExpectingFrameAvailable() override;
  void WaitForFrameAvailable() override;
  std::unique_ptr<gl::GLImage::ScopedHardwareBuffer> GetAHardwareBuffer()
      override;

 private:
  friend class TextureOwner;

  class ScopedHardwareBufferImpl;

  ImageReaderGLOwner(GLuint texture_id);
  ~ImageReaderGLOwner() override;

  // Deletes the current image if it has no pending refs. Returns false on
  // error.
  bool MaybeDeleteCurrentImage();

  void EnsureTexImageBound();
  void ReleaseRefOnImage(AImage* image);

  // AImageReader instance
  AImageReader* image_reader_;

  // Most recently acquired image using image reader. This works like a cached
  // image until next new image is acquired which overwrites this.
  AImage* current_image_;
  base::ScopedFD current_image_fence_;
  GLuint texture_id_;
  std::unique_ptr<AImageReader_ImageListener> listener_;

  // Set to true if the current image is bound to |texture_id_|.
  bool current_image_bound_ = false;

  // A map consisting of pending external refs on an AImage. If an image has any
  // external refs, it is automatically released once the ref-count is 0 and the
  // image is no longer current.
  using AImageRefMap = base::flat_map<AImage*, size_t>;
  AImageRefMap external_image_refs_;

  // reference to the class instance which is used to dynamically
  // load the functions in android libraries at runtime.
  base::android::AndroidImageReader& loader_;

  // The context and surface that were used to create |texture_id_|.
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLSurface> surface_;

  // When SetReleaseTimeToNow() was last called. i.e., when the last
  // codec buffer was released to this surface. Or null if
  // IgnorePendingRelease() or WaitForFrameAvailable() have been called since.
  base::TimeTicks release_time_;
  scoped_refptr<FrameAvailableEvent_ImageReader> frame_available_event_;

  THREAD_CHECKER(thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(ImageReaderGLOwner);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_IMAGE_READER_GL_OWNER_H_
