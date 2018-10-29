// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_TEXTURE_OWNER_H_
#define MEDIA_GPU_ANDROID_TEXTURE_OWNER_H_

#include <android/hardware_buffer.h>

#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/single_thread_task_runner.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_surface.h"

namespace media {

// A Texture wrapper interface that creates and maintains ownership of the
// attached GL or Vulkan texture. The texture is destroyed with the object.
// It should only be accessed on the thread it was created on, with the
// exception of CreateJavaSurface(), which can be called on any thread. It's
// safe to keep and drop refptrs to it on any thread; it will be automatically
// destructed on the thread it was constructed on.
class MEDIA_GPU_EXPORT TextureOwner
    : public base::RefCountedDeleteOnSequence<TextureOwner> {
 public:
  // Creates a GL texture using the current platform GL context and returns a
  // new TextureOwner attached to it. Returns null on failure.
  static scoped_refptr<TextureOwner> Create();

  TextureOwner();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return task_runner_;
  }

  // Returns the GL texture id that the TextureOwner is attached to.
  virtual GLuint GetTextureId() const = 0;
  virtual gl::GLContext* GetContext() const = 0;
  virtual gl::GLSurface* GetSurface() const = 0;

  // Create a java surface for the TextureOwner.
  virtual gl::ScopedJavaSurface CreateJavaSurface() const = 0;

  // Update the texture image using the latest available image data.
  virtual void UpdateTexImage() = 0;

  // Transformation matrix if any associated with the texture image.
  virtual void GetTransformMatrix(float mtx[16]) = 0;
  virtual void ReleaseBackBuffers() = 0;

  // Sets the expectation of onFrameAVailable for a new frame because a buffer
  // was just released to this surface.
  virtual void SetReleaseTimeToNow() = 0;

  // Ignores a pending release that was previously indicated with
  // SetReleaseTimeToNow(). TODO(watk): This doesn't seem necessary. It
  // actually may be detrimental because the next time we release a buffer we
  // may confuse its onFrameAvailable with the one we're ignoring.
  virtual void IgnorePendingRelease() = 0;

  // Whether we're expecting onFrameAvailable. True when SetReleaseTimeToNow()
  // was called but neither IgnorePendingRelease() nor WaitForFrameAvailable()
  // have been called since.
  virtual bool IsExpectingFrameAvailable() = 0;

  // Waits for onFrameAvailable until it's been 5ms since the buffer was
  // released. This must only be called if IsExpectingFrameAvailable().
  virtual void WaitForFrameAvailable() = 0;

  // Retrieves the AHardwareBuffer from the latest available image data.
  // Note that the object must be used and destroyed on the same thread the
  // TextureOwner is bound to.
  virtual std::unique_ptr<gl::GLImage::ScopedHardwareBuffer>
  GetAHardwareBuffer() = 0;

 protected:
  friend class base::RefCountedDeleteOnSequence<TextureOwner>;
  friend class base::DeleteHelper<TextureOwner>;
  virtual ~TextureOwner();

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(TextureOwner);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_TEXTURE_OWNER_H_
