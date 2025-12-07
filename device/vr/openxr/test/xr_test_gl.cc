// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/test/xr_test_gl.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <dlfcn.h>

#include "base/logging.h"

#define LOAD_GL_FN(name, upper_name) \
  name##_fn =                        \
      reinterpret_cast<PFN##upper_name##PROC>(dlsym(lib_gles_handle_, #name))
#define LOAD_EGL_FN(name, upper_name) \
  name##_fn =                         \
      reinterpret_cast<PFN##upper_name##PROC>(dlsym(lib_egl_handle_, #name))

XrTestGl::XrTestGl() {
  lib_egl_handle_ = dlopen("libEGL.so", RTLD_LAZY | RTLD_LOCAL);
  if (!lib_egl_handle_) {
    LOG(ERROR) << "Failed to dlopen libEGL.so: " << dlerror();
    return;
  }

  lib_gles_handle_ = dlopen("libGLESv2.so", RTLD_LAZY | RTLD_LOCAL);
  if (!lib_gles_handle_) {
    LOG(ERROR) << "Failed to dlopen libGLESv2.so: " << dlerror();
    return;
  }

  LOAD_GL_FN(glGenTextures, GLGENTEXTURES);
  LOAD_GL_FN(glBindTexture, GLBINDTEXTURE);
  LOAD_GL_FN(glTexImage2D, GLTEXIMAGE2D);
  LOAD_GL_FN(glGenFramebuffers, GLGENFRAMEBUFFERS);
  LOAD_GL_FN(glBindFramebuffer, GLBINDFRAMEBUFFER);
  LOAD_GL_FN(glFramebufferTexture2D, GLFRAMEBUFFERTEXTURE2D);
  LOAD_GL_FN(glCheckFramebufferStatus, GLCHECKFRAMEBUFFERSTATUS);
  LOAD_GL_FN(glReadPixels, GLREADPIXELS);
  LOAD_GL_FN(glDeleteFramebuffers, GLDELETEFRAMEBUFFERS);

  LOAD_EGL_FN(eglGetCurrentContext, EGLGETCURRENTCONTEXT);
  LOAD_EGL_FN(eglGetCurrentDisplay, EGLGETCURRENTDISPLAY);
  LOAD_EGL_FN(eglGetCurrentSurface, EGLGETCURRENTSURFACE);
  LOAD_EGL_FN(eglGetError, EGLGETERROR);
}

XrTestGl::~XrTestGl() {
  if (lib_gles_handle_) {
    dlclose(lib_gles_handle_);
    lib_gles_handle_ = nullptr;
  }
  if (lib_egl_handle_) {
    dlclose(lib_egl_handle_);
    lib_egl_handle_ = nullptr;
  }
}
#undef LOAD_GL_FN
#undef LOAD_EGL_FN
