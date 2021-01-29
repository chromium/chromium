// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_GLES2_CONFORM_TEST_SURFACE_H_
#define GPU_GLES2_CONFORM_TEST_SURFACE_H_

#include <EGL/egl.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
namespace gl {
class GLSurface;
}
namespace egl {
class Config;

class Surface : public base::RefCountedThreadSafe<Surface> {
 public:
  explicit Surface(gl::GLSurface* gl_surface, const Config* config);
  void set_is_current_in_some_thread(bool flag) {
    is_current_in_some_thread_ = flag;
  }
  bool is_current_in_some_thread() const { return is_current_in_some_thread_; }
  gl::GLSurface* gl_surface() const;
  const Config* config() const;
  static bool ValidatePbufferAttributeList(const EGLint* attrib_list);
  static bool ValidateWindowAttributeList(const EGLint* attrib_list);

 private:
  friend class base::RefCountedThreadSafe<Surface>;
  ~Surface();
  bool is_current_in_some_thread_;
  scoped_refptr<gl::GLSurface> gl_surface_;
  const Config* config_;
  DISALLOW_COPY_AND_ASSIGN(Surface);
};

}  // namespace egl

#endif  // GPU_GLES2_CONFORM_TEST_SURFACE_H_
