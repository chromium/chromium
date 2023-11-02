// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_GLES2_CONFORM_SUPPORT_EGL_THREAD_STATE_H_
#define GPU_GLES2_CONFORM_SUPPORT_EGL_THREAD_STATE_H_

#include <EGL/egl.h>
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"

namespace gles2_conform_support {
namespace egl {

class Context;
class Display;
class Surface;

// Thread-local API state of EGL.
class ThreadState {
 public:
  // Factory getter for the class. Should only be called by the API layer, and
  // then passed through Display in order to avoid lock issues.
  static ThreadState* Get();

  ThreadState(const ThreadState&) = delete;
  ThreadState& operator=(const ThreadState&) = delete;

  static void ReleaseThread();

  Surface* current_surface() const { return current_surface_.get(); }
  Context* current_context() const { return current_context_.get(); }

  template <typename T>
  T ReturnError(EGLint error, T return_value) {
    error_code_ = error;
    return return_value;
  }
  template <typename T>
  T ReturnSuccess(T return_value) {
    error_code_ = EGL_SUCCESS;
    return return_value;
  }
  EGLint ConsumeErrorCode();

  Display* GetDefaultDisplay();
  Display* GetDisplay(EGLDisplay);

  // RAII class for ensuring that ThreadState current context
  // is reflected in the gfx:: and gles:: global variables.
  class AutoCurrentContextRestore {
   public:
    AutoCurrentContextRestore(ThreadState*);

    AutoCurrentContextRestore(const AutoCurrentContextRestore&) = delete;
    AutoCurrentContextRestore& operator=(const AutoCurrentContextRestore&) =
        delete;

    ~AutoCurrentContextRestore();
    void SetCurrent(Surface*, Context*);

   private:
    raw_ptr<ThreadState> thread_state_;
  };

 private:
  ThreadState();
  ~ThreadState();
  void SetCurrent(Surface*, Context*);

  EGLint error_code_;
  scoped_refptr<Surface> current_surface_;
  scoped_refptr<Context> current_context_;
};

}  // namespace egl
}  // namespace gles2_conform_support

#endif  // GPU_GLES2_CONFORM_SUPPORT_EGL_THREAD_STATE_H_
