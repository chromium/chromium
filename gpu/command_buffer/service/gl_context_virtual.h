// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GL_CONTEXT_VIRTUAL_H_
#define GPU_COMMAND_BUFFER_SERVICE_GL_CONTEXT_VIRTUAL_H_

#include <string>
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gl/gl_context.h"

namespace gl {
class GPUTimingClient;
class GLShareGroup;
class GLSurface;
}

namespace gpu {
class GLContextVirtualDelegate;

// Encapsulates a virtual OpenGL context.
class GPU_GLES2_EXPORT GLContextVirtual : public gl::GLContext {
 public:
  GLContextVirtual(gl::GLShareGroup* share_group,
                   gl::GLContext* shared_context,
                   base::WeakPtr<GLContextVirtualDelegate> delegate);

  GLContextVirtual(const GLContextVirtual&) = delete;
  GLContextVirtual& operator=(const GLContextVirtual&) = delete;

  // Implement GLContext.
  bool InitializeImpl(gl::GLSurface* compatible_surface,
                      const gl::GLContextAttribs& attribs) override;
  bool MakeCurrentImpl(gl::GLSurface* surface) override;
  void ReleaseCurrent(gl::GLSurface* surface) override;
  bool IsCurrent(gl::GLSurface* surface) override;
  void* GetHandle() override;
  scoped_refptr<gl::GPUTimingClient> CreateGPUTimingClient() override;
  std::string GetGLVersion() override;
  std::string GetGLRenderer() override;
  const gfx::ExtensionSet& GetExtensions() override;
  void SetSafeToForceGpuSwitch() override;
  unsigned int CheckStickyGraphicsResetStatusImpl() override;
  void SetUnbindFboOnMakeCurrent() override;
  void ForceReleaseVirtuallyCurrent() override;
#if BUILDFLAG(IS_APPLE)
  void AddMetalSharedEventsForBackpressure(
      std::vector<std::unique_ptr<BackpressureMetalSharedEvent>> events)
      override;
  uint64_t BackpressureFenceCreate() override;
  void BackpressureFenceWait(uint64_t fence) override;
#endif
#if BUILDFLAG(IS_MAC)
  void FlushForDriverCrashWorkaround() override;
#endif

  gl::GLDisplayEGL* GetGLDisplayEGL() override;

 protected:
  ~GLContextVirtual() override;
  void ResetExtensions() override;

 private:
  void Destroy();

  scoped_refptr<gl::GLContext> shared_context_;
  base::WeakPtr<GLContextVirtualDelegate> delegate_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GL_CONTEXT_VIRTUAL_H_
