// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/gles2_conform_support/egl/surface.h"
#include "ui/gl/gl_surface.h"

namespace gles2_conform_support {
namespace egl {

Surface::Surface(gl::GLSurface* gl_surface, const Config* config)
    : is_current_in_some_thread_(false),
      gl_surface_(gl_surface),
      config_(config) {}

Surface::~Surface() = default;

gl::GLSurface* Surface::gl_surface() const {
  return gl_surface_.get();
}

const Config* Surface::config() const {
  return config_;
}

bool Surface::ValidatePbufferAttributeList(const EGLint* attrib_list) {
  if (attrib_list) {
    for (int i = 0; attrib_list[i] != EGL_NONE; i += 2) {
      switch (attrib_list[i]) {
        case EGL_WIDTH:
        case EGL_HEIGHT:
          break;
        default:
          return false;
      }
    }
  }
  return true;
}

bool Surface::ValidateWindowAttributeList(const EGLint* attrib_list) {
  if (attrib_list) {
    if (attrib_list[0] != EGL_NONE)
      return false;
  }
  return true;
}
}  // namespace egl
}  // namespace gles2_conform_support
