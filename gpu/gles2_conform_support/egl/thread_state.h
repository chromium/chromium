// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_GLES2_CONFORM_SUPPORT_EGL_STATE_H_
#define GPU_GLES2_CONFORM_SUPPORT_EGL_STATE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include <EGL/egl.h>

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
    ~AutoCurrentContextRestore();
    void SetCurrent(Surface*, Context*);

   private:
    ThreadState* thread_state_;
    DISALLOW_COPY_AND_ASSIGN(AutoCurrentContextRestore);
  };

 private:
  ThreadState();
  ~ThreadState();
  void SetCurrent(Surface*, Context*);

  EGLint error_code_;
  scoped_refptr<Surface> current_surface_;
  scoped_refptr<Context> current_context_;
  DISALLOW_COPY_AND_ASSIGN(ThreadState);
};

}  // namespace egl

#endif
