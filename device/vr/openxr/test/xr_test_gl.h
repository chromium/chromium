// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_TEST_XR_TEST_GL_H_
#define DEVICE_VR_OPENXR_TEST_XR_TEST_GL_H_

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "base/memory/raw_ptr.h"

// The mock OpenXR runtime is built as a standalone shared library. Because of
// this many helpers for gl/egl can't be used as they expect quite a lot of
// chrome infrastructure (command line, feature flags, RunLoop), to be setup.
// This class is intended to help serve as an abstraction for the openxr mock
// runtime to load/open the minimal set of GL/EGL functions needed by the tests.
// Each instance of the class currently opens a new handle to the libraries.
class XrTestGl {
 public:
  XrTestGl();
  virtual ~XrTestGl();

  // GLES function pointers
  PFNGLGENTEXTURESPROC glGenTextures_fn = nullptr;
  PFNGLBINDTEXTUREPROC glBindTexture_fn = nullptr;
  PFNGLTEXIMAGE2DPROC glTexImage2D_fn = nullptr;
  PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers_fn = nullptr;
  PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer_fn = nullptr;
  PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D_fn = nullptr;
  PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus_fn = nullptr;
  PFNGLREADPIXELSPROC glReadPixels_fn = nullptr;
  PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers_fn = nullptr;

  // EGL function pointers
  PFNEGLGETCURRENTCONTEXTPROC eglGetCurrentContext_fn = nullptr;
  PFNEGLGETCURRENTDISPLAYPROC eglGetCurrentDisplay_fn = nullptr;
  PFNEGLGETCURRENTSURFACEPROC eglGetCurrentSurface_fn = nullptr;
  PFNEGLGETERRORPROC eglGetError_fn = nullptr;

 private:
  raw_ptr<void> lib_gles_handle_ = nullptr;
  raw_ptr<void> lib_egl_handle_ = nullptr;
};
#endif  // DEVICE_VR_OPENXR_TEST_XR_TEST_GL_H_
