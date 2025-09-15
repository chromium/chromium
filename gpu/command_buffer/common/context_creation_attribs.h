// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_CONTEXT_CREATION_ATTRIBS_H_
#define GPU_COMMAND_BUFFER_COMMON_CONTEXT_CREATION_ATTRIBS_H_

#include <stdint.h>

#include "build/build_config.h"
#include "gpu/command_buffer/common/gpu_command_buffer_common_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gpu_preference.h"

namespace gpu {

enum ContextType {
  CONTEXT_TYPE_WEBGL1,
  CONTEXT_TYPE_WEBGL2,
  CONTEXT_TYPE_OPENGLES2,
  CONTEXT_TYPE_OPENGLES3,
  CONTEXT_TYPE_OPENGLES31_FOR_TESTING,
  CONTEXT_TYPE_LAST = CONTEXT_TYPE_OPENGLES31_FOR_TESTING
};

GPU_COMMAND_BUFFER_COMMON_EXPORT bool IsWebGLContextType(
    ContextType context_type);
GPU_COMMAND_BUFFER_COMMON_EXPORT bool IsWebGL1OrES2ContextType(
    ContextType context_type);
GPU_COMMAND_BUFFER_COMMON_EXPORT bool IsWebGL2OrES3ContextType(
    ContextType context_type);
GPU_COMMAND_BUFFER_COMMON_EXPORT bool IsWebGL2OrES3OrHigherContextType(
    ContextType context_type);
GPU_COMMAND_BUFFER_COMMON_EXPORT bool IsES31ForTestingContextType(
    ContextType context_type);
GPU_COMMAND_BUFFER_COMMON_EXPORT const char* ContextTypeToLabel(
    ContextType context_type);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_CONTEXT_CREATION_ATTRIBS_H_
