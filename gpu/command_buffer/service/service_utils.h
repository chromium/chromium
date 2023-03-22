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

namespace gles2 {
class ContextGroup;

GPU_GLES2_EXPORT gl::GLContextAttribs GenerateGLContextAttribs(
    const ContextCreationAttribs& attribs_helper,
    const ContextGroup* context_group);

GPU_GLES2_EXPORT gl::GLContextAttribs GenerateGLContextAttribs(
    const ContextCreationAttribs& attribs_helper,
    bool use_passthrough_cmd_decoder);

// Returns true if the passthrough command decoder has been requested
GPU_GLES2_EXPORT bool UsePassthroughCommandDecoder(
    const base::CommandLine* command_line);

// Returns true if the driver supports creating passthrough command decoders
GPU_GLES2_EXPORT bool PassthroughCommandDecoderSupported();

GPU_GLES2_EXPORT GpuPreferences
ParseGpuPreferences(const base::CommandLine* command_line);

// Determine which Skia GrContext backend will be used for GPU compositing and
// rasterization (if enabled) by checking the feature flags for Vulkan and
// Metal. If they are not enabled, default to GL.
GPU_GLES2_EXPORT GrContextType ParseGrContextType();

// Parse the value of --use-vulkan from the command line. If unspecified and
// features::kVulkan is enabled (GrContext is going to use vulkan), default to
// the native implementation.
GPU_GLES2_EXPORT VulkanImplementationName
ParseVulkanImplementationName(const base::CommandLine* command_line);

GPU_GLES2_EXPORT WebGPUAdapterName
ParseWebGPUAdapterName(const base::CommandLine* command_line);

GPU_GLES2_EXPORT WebGPUPowerPreference
ParseWebGPUPowerPreference(const base::CommandLine* command_line);

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SERVICE_UTILS_H_
