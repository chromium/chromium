// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SURFACE_TEXTURE_GL_OWNER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SURFACE_TEXTURE_GL_OWNER_H_

#include "base/threading/thread_checker.h"
#include "gpu/command_buffer/service/texture_owner.h"
#include "gpu/gpu_export.h"
#include "ui/gl/android/surface_texture.h"

namespace base {
namespace android {
class ScopedHardwareBufferFenceSync;
}  // namespace android
}  // namespace base

namespace gpu {

// This class wraps the Surface Texture usage. It is used to create a surface
// texture attached to a new texture of the current platform GL context. The
// surface handle of the SurfaceTexture is attached to the decoded media
// frames. Media frames can update the attached surface handle with image data.
// This class helps to update the attached texture using that image data
// present in the surface.
class GPU_GLES2_EXPORT SurfaceTextureGLOwner : public TextureOwner {
 public:
  SurfaceTextureGLOwner(const SurfaceTextureGLOwner&) = delete;
  SurfaceTextureGLOwner& operator=(const SurfaceTextureGLOwner&) = delete;

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

  // MemoryDumpProvider:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 protected:
  void ReleaseResources() override;

 private:
  friend class TextureOwner;
  friend class SurfaceTextureGLOwnerTest;
  friend class SurfaceTextureTransformTest;

  SurfaceTextureGLOwner(std::unique_ptr<AbstractTextureAndroid> texture,
                        scoped_refptr<SharedContextState> context_state);
  ~SurfaceTextureGLOwner() override;

  static bool DecomposeTransform(float matrix[16],
                                 gfx::Size rotated_visible_size,
                                 gfx::Size* coded_size,
                                 gfx::Rect* visible_rect);

  scoped_refptr<gl::SurfaceTexture> surface_texture_;

  // The context and surface that were used to create |surface_texture_|.
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLSurface> surface_;

  // To ensure that SetFrameAvailableCallback() is called only once.
  bool is_frame_available_callback_set_ = false;

  // This is not precise, but good estimation for memory dumps.
  std::optional<gfx::Size> last_coded_size_for_memory_dumps_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SURFACE_TEXTURE_GL_OWNER_H_
