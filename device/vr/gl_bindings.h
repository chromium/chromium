// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_GL_BINDINGS_H_
#define DEVICE_VR_GL_BINDINGS_H_

#if defined(VR_USE_COMMAND_BUFFER)

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#elif defined(VR_USE_NATIVE_GL)

#include "ui/gl/gl_bindings.h"  // nogncheck

// The above header still uses the ARB prefix for the following GL API call.
#define glGenBuffers glGenBuffersARB

#else

#error "Missing configuration for GL mode."

#endif  // defined(VR_USE_COMMAND_BUFFER)

#endif  // DEVICE_VR_GL_BINDINGS_H_
