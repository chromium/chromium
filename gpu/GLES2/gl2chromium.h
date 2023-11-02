// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains Chromium-specific GLES2 declarations.

#ifndef GPU_GLES2_GL2CHROMIUM_H_
#define GPU_GLES2_GL2CHROMIUM_H_

#include <GLES2/gl2platform.h>

#define GL_CONTEXT_LOST 0x300E

#if defined(GLES2_USE_MOJO)
#define GLES2_GET_FUN(name) MojoGLES2gl ## name
#else
// Because we are using both the real system GL and our own
// emulated GL we need to use different names to avoid conflicts.
#if defined(GLES2_USE_CPP_BINDINGS)
#define GLES2_GET_FUN(name) gles2::GetGLContext()->name
#else
#define GLES2_GET_FUN(name) GLES2 ## name
#endif
#endif

#include <GLES2/gl2chromium_autogen.h>

#endif  // GPU_GLES2_GL2CHROMIUM_H_

