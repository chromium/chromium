// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_GLES2_CONFORM_SUPPORT_EGL_DISPLAY_H_
#define GPU_GLES2_CONFORM_SUPPORT_EGL_DISPLAY_H_

#include <EGL/egl.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"

namespace gles2_conform_support::egl {

class Config;
class Context;
class Surface;
class ThreadState;

class Display {
 public:
  explicit Display();

  Display(const Display&) = delete;
  Display& operator=(const Display&) = delete;

  ~Display();

  bool is_initialized() const { return is_initialized_; }

  void ReleaseCurrentForReleaseThread(ThreadState*);

  // A function for windowless GTF tests.
  void SetNextCreateWindowSurfaceCreatesPBuffer(EGLint width, EGLint height);

  EGLBoolean Initialize(ThreadState* ts, EGLint* major, EGLint* minor);
  EGLBoolean Terminate(ThreadState* ts);
  const char* QueryString(ThreadState* ts, EGLint name);

  // Config routines.
  EGLBoolean GetConfigAttrib(ThreadState* ts,
                             EGLConfig cfg,
                             EGLint attribute,
                             EGLint* value);
  EGLBoolean ChooseConfig(ThreadState* ts,
                          const EGLint* attrib_list,
                          EGLConfig* configs,
                          EGLint config_size,
                          EGLint* num_config);
  EGLBoolean GetConfigs(ThreadState*,
                        EGLConfig*,
                        EGLint config_size,
                        EGLint* num_config);

  // Surface routines.
  static bool IsValidNativeWindow(EGLNativeWindowType);
  EGLSurface CreatePbufferSurface(ThreadState*,
                                  EGLConfig,
                                  const EGLint* attrib_list);
  EGLSurface CreateWindowSurface(ThreadState*,
                                 EGLConfig,
                                 EGLNativeWindowType win,
                                 const EGLint* attrib_list);
  EGLBoolean DestroySurface(ThreadState*, EGLSurface);
  EGLBoolean SwapBuffers(ThreadState*, EGLSurface);

  // Context routines.
  EGLContext CreateContext(ThreadState*,
                           EGLConfig,
                           EGLSurface share_ctx,
                           const EGLint* attrib_list);
  EGLBoolean DestroyContext(ThreadState*, EGLContext);

  EGLBoolean ReleaseCurrent(ThreadState*);
  EGLBoolean MakeCurrent(ThreadState*, EGLSurface, EGLSurface, EGLContext);

  uint64_t GenerateFenceSyncRelease();

 private:
  void InitializeConfigsIfNeeded();
  const Config* GetConfig(EGLConfig);
  Surface* GetSurface(EGLSurface);
  Context* GetContext(EGLContext);
  EGLSurface DoCreatePbufferSurface(ThreadState* ts,
                                    const Config* config,
                                    EGLint width,
                                    EGLint height);

  base::Lock lock_;
  bool is_initialized_;
  uint64_t next_fence_sync_release_;
  std::vector<scoped_refptr<Surface>> surfaces_;
  std::vector<scoped_refptr<Context>> contexts_;
  std::unique_ptr<Config> configs_[2];

  // GTF windowless support.
  bool next_create_window_surface_creates_pbuffer_;
  EGLint window_surface_pbuffer_width_;
  EGLint window_surface_pbuffer_height_;
};

}  // namespace gles2_conform_support::egl

#endif  // GPU_GLES2_CONFORM_SUPPORT_EGL_DISPLAY_H_
