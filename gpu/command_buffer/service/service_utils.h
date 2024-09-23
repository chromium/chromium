// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SERVICE_UTILS_H_
#define GPU_COMMAND_BUFFER_SERVICE_SERVICE_UTILS_H_

#include "base/command_line.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gl/gl_context.h"

namespace gpu {
struct ContextCreationAttribs;
class GpuDriverBugWorkarounds;

namespace gles2 {
class ContextGroup;

GPU_GLES2_EXPORT gl::GLContextAttribs GenerateGLContextAttribsForDecoder(
    const ContextCreationAttribs& attribs_helper,
    const ContextGroup* context_group);

GPU_GLES2_EXPORT gl::GLContextAttribs GenerateGLContextAttribsForCompositor(
    bool use_passthrough_cmd_decoder);

// Returns true if the passthrough command decoder has been requested
GPU_GLES2_EXPORT bool UsePassthroughCommandDecoder(
    const base::CommandLine* command_line);

// Returns true if the driver supports creating passthrough command decoders
GPU_GLES2_EXPORT bool PassthroughCommandDecoderSupported();

GPU_GLES2_EXPORT GpuPreferences
ParseGpuPreferences(const base::CommandLine* command_line);

// Determine which Skia GrContext backend will be used for GPU compositing and
// rasterization (if enabled) by checking the feature flags for Vulkan and/or
// Graphite. If they are not enabled, default to GL.
// If Graphite is enabled, the backend is Dawn by default or cn be specified
// using the --skia-graphite-backend flag. On iOS, the backend is Metal by
// default if skia_use_metal is set to true via gn args.
GPU_GLES2_EXPORT GrContextType
ParseGrContextType(const base::CommandLine* command_line);

bool MSAAIsSlow(const GpuDriverBugWorkarounds& workarounds);

}  // namespace gles2

#if BUILDFLAG(IS_MAC)
// Gets the texture target to use with MacOS native GpuMemoryBuffers based on
// the current GL implementation.
GPU_GLES2_EXPORT uint32_t GetTextureTargetForIOSurfaces();
#endif  // BUILDFLAG(IS_MAC)

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SERVICE_UTILS_H_
