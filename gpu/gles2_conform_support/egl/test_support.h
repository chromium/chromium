// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_GLES2_CONFORM_SUPPORT_EGL_TEST_SUPPORT_H_
#define GPU_GLES2_CONFORM_SUPPORT_EGL_TEST_SUPPORT_H_

#include <EGL/egl.h>

#if defined(COMPONENT_BUILD) && defined(COMMAND_BUFFER_GLES_LIB_SUPPORT_ONLY)
// A variable used for communicating whether the app has initialized the global
// variables.
// On component build, the dynamic library and the Chromium test
// runner executable refer to the same global variables. Any non-Chromium client
// of the dynamic library will not initialize the globabl variables.
// On non-component (static) build, the library and the runner have distinct
// global variables.
EGLAPI extern EGLAPIENTRY bool g_command_buffer_gles_has_atexit_manager;
#endif

extern "C" {
// A function to support GTF windowless tests. gles2_conform_test_windowless and
// khronos_glcts_test_windowless create "windowless" native windows and render
// to those. The test runners do not at the moment implement creating said
// windowless native windows. This call sets the system so that it will create a
// pbuffer when eglCreateWindow is called.
EGLAPI EGLAPIENTRY void
CommandBufferGLESSetNextCreateWindowSurfaceCreatesPBuffer(EGLDisplay eglDisplay,
                                                          EGLint width,
                                                          EGLint height);
}
#endif  // GPU_GLES2_CONFORM_SUPPORT_EGL_TEST_SUPPORT_H_
