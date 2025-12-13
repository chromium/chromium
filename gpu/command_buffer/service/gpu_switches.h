// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by gpu/command_buffer/service/.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_SWITCHES_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_SWITCHES_H_

#include "gpu/command_buffer/service/gpu_command_buffer_service_export.h"
#include "gpu/config/gpu_switches.h"

namespace switches {

GPU_COMMAND_BUFFER_SERVICE_EXPORT extern const char
    kCompileShaderAlwaysSucceeds[];
GPU_COMMAND_BUFFER_SERVICE_EXPORT extern const char kDisableGLErrorLimit[];
GPU_COMMAND_BUFFER_SERVICE_EXPORT extern const char kDisableGLSLTranslator[];
GPU_COMMAND_BUFFER_SERVICE_EXPORT extern const char kDisableShaderNameHashing[];
GPU_COMMAND_BUFFER_SERVICE_EXPORT extern const char kEnableGPUCommandLogging[];
GPU_COMMAND_BUFFER_SERVICE_EXPORT extern const char kEnableGPUDebugging[];
GPU_COMMAND_BUFFER_SERVICE_EXPORT extern const char
    kEnableGPUServiceLoggingGPU[];
GPU_COMMAND_BUFFER_SERVICE_EXPORT extern const char
    kEnableGPUDriverDebugLogging[];
GPU_COMMAND_BUFFER_SERVICE_EXPORT extern const char kDisableGpuProgramCache[];
GPU_COMMAND_BUFFER_SERVICE_EXPORT extern const char kEnforceGLMinimums[];
GPU_COMMAND_BUFFER_SERVICE_EXPORT extern const char
    kForceGpuMemDiscardableLimitMb[];
GPU_COMMAND_BUFFER_SERVICE_EXPORT extern const char kForceMaxTextureSize[];
GPU_COMMAND_BUFFER_SERVICE_EXPORT extern const char kGpuProgramCacheSizeKb[];
GPU_COMMAND_BUFFER_SERVICE_EXPORT extern const char
    kEnableThreadedTextureMailboxes[];
GPU_COMMAND_BUFFER_SERVICE_EXPORT extern const char kGLShaderIntermOutput[];
GPU_COMMAND_BUFFER_SERVICE_EXPORT extern const char kUseVulkan[];
GPU_COMMAND_BUFFER_SERVICE_EXPORT extern const char
    kVulkanImplementationNameNative[];
GPU_COMMAND_BUFFER_SERVICE_EXPORT extern const char
    kVulkanImplementationNameSwiftshader[];
GPU_COMMAND_BUFFER_SERVICE_EXPORT extern const char kDisableVulkanSurface[];

}  // namespace switches

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_SWITCHES_H_
