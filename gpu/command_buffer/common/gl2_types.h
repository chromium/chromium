// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_GL2_TYPES_H_
#define GPU_COMMAND_BUFFER_COMMON_GL2_TYPES_H_

#include <KHR/khrplatform.h>

// GL types are forward declared to avoid including the GL headers. The problem
// is determining which GL headers to include from code that is common to the
// client and service sides (GLES2 or one of several GL implementations).
typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef signed char GLbyte;
typedef short GLshort;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned long GLulong;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef void GLvoid;
typedef khronos_intptr_t GLintptr;
typedef khronos_ssize_t GLsizeiptr;
typedef struct __GLsync* GLsync;
typedef int64_t GLint64;
typedef uint64_t GLuint64;

#endif  // GPU_COMMAND_BUFFER_COMMON_GL2_TYPES_H_
