// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_CONTEXT_CREATION_ATTRIBS_H_
#define GPU_COMMAND_BUFFER_COMMON_CONTEXT_CREATION_ATTRIBS_H_

#include <stdint.h>

#include "gpu/gpu_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gpu_preference.h"

namespace gpu {

enum ContextType {
  CONTEXT_TYPE_WEBGL1,
  CONTEXT_TYPE_WEBGL2,
  CONTEXT_TYPE_WEBGL2_COMPUTE,
  CONTEXT_TYPE_OPENGLES2,
  CONTEXT_TYPE_OPENGLES3,
  CONTEXT_TYPE_WEBGPU,
  CONTEXT_TYPE_LAST = CONTEXT_TYPE_WEBGPU
};

GPU_EXPORT bool IsGLContextType(ContextType context_type);
GPU_EXPORT bool IsWebGLContextType(ContextType context_type);
GPU_EXPORT bool IsWebGL1OrES2ContextType(ContextType context_type);
GPU_EXPORT bool IsWebGL2OrES3ContextType(ContextType context_type);
GPU_EXPORT bool IsWebGL2OrES3OrHigherContextType(ContextType context_type);
GPU_EXPORT bool IsWebGL2ComputeContextType(ContextType context_type);
GPU_EXPORT bool IsWebGPUContextType(ContextType context_type);

enum ColorSpace {
  COLOR_SPACE_UNSPECIFIED,
  COLOR_SPACE_SRGB,
  COLOR_SPACE_DISPLAY_P3,
  COLOR_SPACE_LAST = COLOR_SPACE_DISPLAY_P3
};

struct GPU_EXPORT ContextCreationAttribs {
  ContextCreationAttribs();
  ContextCreationAttribs(const ContextCreationAttribs& other);
  ContextCreationAttribs& operator=(const ContextCreationAttribs& other);

  gfx::Size offscreen_framebuffer_size;
  gl::GpuPreference gpu_preference = gl::GpuPreference::kLowPower;
  // -1 if invalid or unspecified.
  int32_t alpha_size = -1;
  int32_t blue_size = -1;
  int32_t green_size = -1;
  int32_t red_size = -1;
  int32_t depth_size = -1;
  int32_t stencil_size = -1;
  int32_t samples = -1;
  int32_t sample_buffers = -1;
  bool buffer_preserved = true;
  bool bind_generates_resource = true;
  bool fail_if_major_perf_caveat = false;
  bool lose_context_when_out_of_memory = false;
  bool should_use_native_gmb_for_backbuffer = false;
  bool own_offscreen_surface = false;
  bool single_buffer = false;
  bool enable_gles2_interface = true;
  bool enable_raster_interface = false;
  bool enable_oop_rasterization = false;
  bool enable_swap_timestamps_if_supported = false;

  ContextType context_type = CONTEXT_TYPE_OPENGLES2;
  ColorSpace color_space = COLOR_SPACE_UNSPECIFIED;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_CONTEXT_CREATION_ATTRIBS_H_
