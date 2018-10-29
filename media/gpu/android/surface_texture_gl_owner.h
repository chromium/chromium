// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_SURFACE_TEXTURE_GL_OWNER_H_
#define MEDIA_GPU_ANDROID_SURFACE_TEXTURE_GL_OWNER_H_

#include "media/gpu/android/texture_owner.h"

#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gl/android/surface_texture.h"

namespace media {

struct FrameAvailableEvent;

// This class wraps the Surface Texture usage. It is used to create a surface
// texture attached to a new texture of the current platform GL context. The
// surface handle of the SurfaceTexture is attached to the decoded media
// frames. Media frames can update the attached surface handle with image data.
// This class helps to update the attached texture using that image data
// present in the surface.
class MEDIA_GPU_EXPORT SurfaceTextureGLOwner : public TextureOwner {
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

  SurfaceTextureGLOwner(GLuint texture_id);
  ~SurfaceTextureGLOwner() override;

  scoped_refptr<gl::SurfaceTexture> surface_texture_;
  GLuint texture_id_;
  // The context and surface that were used to create |texture_id_|.
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLSurface> surface_;
  // When SetReleaseTimeToNow() was last called. i.e., when the last
  // codec buffer was released to this surface. Or null if
  // IgnorePendingRelease() or WaitForFrameAvailable() have been called since.
  base::TimeTicks release_time_;
  scoped_refptr<FrameAvailableEvent> frame_available_event_;

  THREAD_CHECKER(thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(SurfaceTextureGLOwner);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_SURFACE_TEXTURE_GL_OWNER_H_
