// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SERVICE_UTILS_H_
#define GPU_COMMAND_BUFFER_SERVICE_SERVICE_UTILS_H_

#include "base/command_line.h"
#include "base/memory/memory_pressure_listener.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gl/gl_context.h"

namespace gpu {
class GpuDriverBugWorkarounds;

namespace gles2 {
class ContextGroup;

GPU_GLES2_EXPORT gl::GLContextAttribs GenerateGLContextAttribsForDecoder(
    ContextType context_type,
    gl::GpuPreference gpu_preference,
    const ContextGroup* context_group);

GPU_GLES2_EXPORT gl::GLContextAttribs GenerateGLContextAttribsForCompositor(
    bool use_passthrough_cmd_decoder);

// Returns true if the passthrough command decoder has been requested
GPU_GLES2_EXPORT bool UsePassthroughCommandDecoder(
    const base::CommandLine* command_line);

GPU_GLES2_EXPORT GpuPreferences
ParseGpuPreferences(const base::CommandLine* command_line);

GPU_GLES2_EXPORT GrContextType
ParseDefaultGrContextType(const base::CommandLine* command_line);

bool MSAAIsSlow(const GpuDriverBugWorkarounds& workarounds);

}  // namespace gles2

#if BUILDFLAG(IS_MAC)
// Gets the texture target to use with MacOS native GpuMemoryBuffers based on
// the current GL implementation.
GPU_GLES2_EXPORT uint32_t GetTextureTargetForIOSurfaces();
#endif  // BUILDFLAG(IS_MAC)

GPU_GLES2_EXPORT size_t UpdateShaderCacheSizeOnMemoryPressure(
    size_t max_cache_size,
    base::MemoryPressureLevel memory_pressure_level);

// Returns `max_cache_size` scaled according to the `memory_limit` (expressed as
// a percentage) received from the memory coordinator. Supports values over
// 100%, but does not support negative values.
GPU_GLES2_EXPORT size_t
UpdateShaderCacheSizeOnMemoryLimit(size_t max_cache_size, int memory_limit);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SERVICE_UTILS_H_
