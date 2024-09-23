// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_CONTEXT_CREATION_ATTRIBS_H_
#define GPU_COMMAND_BUFFER_COMMON_CONTEXT_CREATION_ATTRIBS_H_

#include <stdint.h>

#include "build/build_config.h"
#include "gpu/gpu_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gpu_preference.h"

namespace gpu {

enum ContextType {
  CONTEXT_TYPE_WEBGL1,
  CONTEXT_TYPE_WEBGL2,
  CONTEXT_TYPE_OPENGLES2,
  CONTEXT_TYPE_OPENGLES3,
  CONTEXT_TYPE_OPENGLES31_FOR_TESTING,
  CONTEXT_TYPE_WEBGPU,
  CONTEXT_TYPE_LAST = CONTEXT_TYPE_WEBGPU
};

GPU_EXPORT bool IsGLContextType(ContextType context_type);
GPU_EXPORT bool IsWebGLContextType(ContextType context_type);
GPU_EXPORT bool IsWebGL1OrES2ContextType(ContextType context_type);
GPU_EXPORT bool IsWebGL2OrES3ContextType(ContextType context_type);
GPU_EXPORT bool IsWebGL2OrES3OrHigherContextType(ContextType context_type);
GPU_EXPORT bool IsES31ForTestingContextType(ContextType context_type);
GPU_EXPORT bool IsWebGPUContextType(ContextType context_type);
GPU_EXPORT const char* ContextTypeToLabel(ContextType context_type);

struct GPU_EXPORT ContextCreationAttribs {
  ContextCreationAttribs();
  ContextCreationAttribs(const ContextCreationAttribs& other);
  ContextCreationAttribs& operator=(const ContextCreationAttribs& other);

  gl::GpuPreference gpu_preference = gl::GpuPreference::kLowPower;

  bool bind_generates_resource = true;
  bool fail_if_major_perf_caveat = false;
  bool lose_context_when_out_of_memory = false;
  bool enable_gles2_interface = true;
  bool enable_grcontext = false;
  bool enable_raster_interface = false;
  bool enable_oop_rasterization = false;

  ContextType context_type = CONTEXT_TYPE_OPENGLES2;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_CONTEXT_CREATION_ATTRIBS_H_
