// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_SKIA_BINDINGS_GL_BINDINGS_SKIA_CMD_BUFFER_H_
#define GPU_SKIA_BINDINGS_GL_BINDINGS_SKIA_CMD_BUFFER_H_

#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypes.h"

struct GrGLInterface;

namespace gpu {
class ContextSupport;
namespace gles2 {
class GLES2Interface;
}  // namespace gles2
}  // namespace gpu

namespace skia_bindings {

// The GPU back-end for skia requires pointers to GL functions. This function
// initializes bindings for skia-gpu to a GLES2Interface object.
sk_sp<GrGLInterface> CreateGLES2InterfaceBindings(
    gpu::gles2::GLES2Interface*,
    gpu::ContextSupport* context_support);

}  // namespace skia_bindings

#endif  // GPU_SKIA_BINDINGS_GL_BINDINGS_SKIA_CMD_BUFFER_H_
