// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_DISPLAY_SYS_OPENGL_H_
#define REMOTING_CLIENT_DISPLAY_SYS_OPENGL_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_IOS)
#include <OpenGLES/ES3/gl.h>
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#elif BUILDFLAG(IS_MAC)
#define GL_GLEXT_PROTOTYPES
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#else
#include <GLES3/gl3.h>
#endif  // BUILDFLAG(IS_IOS)

#endif  // REMOTING_CLIENT_DISPLAY_SYS_OPENGL_H_
