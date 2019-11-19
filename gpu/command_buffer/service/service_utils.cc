// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/service_utils.h"

#include <string>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_finch_features.h"
#include "skia/buildflags.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"

#if defined(USE_EGL)
#include "ui/gl/gl_surface_egl.h"
#endif  // defined(USE_EGL)

namespace gpu {
namespace gles2 {

namespace {

bool GetUintFromSwitch(const base::CommandLine* command_line,
                       const base::StringPiece& switch_string,
                       uint32_t* value) {
  if (!command_line->HasSwitch(switch_string))
    return false;
  std::string switch_value(command_line->GetSwitchValueASCII(switch_string));
  return base::StringToUint(switch_value, value);
}

}  // namespace

gl::GLContextAttribs GenerateGLContextAttribs(
    const ContextCreationAttribs& attribs_helper,
    const ContextGroup* context_group) {
  return GenerateGLContextAttribs(attribs_helper,
                                  context_group->use_passthrough_cmd_decoder());
}

gl::GLContextAttribs GenerateGLContextAttribs(
    const ContextCreationAttribs& attribs_helper,
    bool use_passthrough_cmd_decoder) {
  gl::GLContextAttribs attribs;
  attribs.gpu_preference = attribs_helper.gpu_preference;
  if (use_passthrough_cmd_decoder) {
    attribs.bind_generates_resource = attribs_helper.bind_generates_resource;
    attribs.webgl_compatibility_context =
        IsWebGLContextType(attribs_helper.context_type);

    // Always use the global texture share group for the passthrough command
    // decoder
    attribs.global_texture_share_group = true;

    attribs.robust_resource_initialization = true;
    attribs.robust_buffer_access = true;

    // Request a specific context version instead of always 3.0
    if (IsWebGL2ComputeContextType(attribs_helper.context_type)) {
      attribs.client_major_es_version = 3;
      attribs.client_minor_es_version = 1;
    } else if (IsWebGL2OrES3ContextType(attribs_helper.context_type)) {
      attribs.client_major_es_version = 3;
      attribs.client_minor_es_version = 0;
    } else {
      DCHECK(IsWebGL1OrES2ContextType(attribs_helper.context_type));
      attribs.client_major_es_version = 2;
      attribs.client_minor_es_version = 0;
    }
  } else {
    attribs.client_major_es_version = 3;
    attribs.client_minor_es_version = 0;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableES3GLContext)) {
    // Forcefully disable ES3 contexts
    attribs.client_major_es_version = 2;
    attribs.client_minor_es_version = 0;
  }

  return attribs;
}

bool UsePassthroughCommandDecoder(const base::CommandLine* command_line) {
  return gl::UsePassthroughCommandDecoder(command_line);
}

bool PassthroughCommandDecoderSupported() {
#if defined(USE_EGL)
  // Using the passthrough command buffer requires that specific ANGLE
  // extensions are exposed
  return gl::GLSurfaceEGL::IsCreateContextBindGeneratesResourceSupported() &&
         gl::GLSurfaceEGL::IsCreateContextWebGLCompatabilitySupported() &&
         gl::GLSurfaceEGL::IsRobustResourceInitSupported() &&
         gl::GLSurfaceEGL::IsDisplayTextureShareGroupSupported() &&
         gl::GLSurfaceEGL::IsCreateContextClientArraysSupported();
#else
  // The passthrough command buffer is only supported on top of ANGLE/EGL
  return false;
#endif  // defined(USE_EGL)
}

GpuPreferences ParseGpuPreferences(const base::CommandLine* command_line) {
  GpuPreferences gpu_preferences;
  gpu_preferences.compile_shader_always_succeeds =
      command_line->HasSwitch(switches::kCompileShaderAlwaysSucceeds);
  gpu_preferences.disable_gl_error_limit =
      command_line->HasSwitch(switches::kDisableGLErrorLimit);
  gpu_preferences.disable_glsl_translator =
      command_line->HasSwitch(switches::kDisableGLSLTranslator);
  gpu_preferences.disable_shader_name_hashing =
      command_line->HasSwitch(switches::kDisableShaderNameHashing);
  gpu_preferences.enable_gpu_command_logging =
      command_line->HasSwitch(switches::kEnableGPUCommandLogging);
  gpu_preferences.enable_gpu_debugging =
      command_line->HasSwitch(switches::kEnableGPUDebugging);
  gpu_preferences.enable_gpu_service_logging_gpu =
      command_line->HasSwitch(switches::kEnableGPUServiceLoggingGPU);
  gpu_preferences.enable_gpu_driver_debug_logging =
      command_line->HasSwitch(switches::kEnableGPUDriverDebugLogging);
  gpu_preferences.disable_gpu_program_cache =
      command_line->HasSwitch(switches::kDisableGpuProgramCache);
  gpu_preferences.enforce_gl_minimums =
      command_line->HasSwitch(switches::kEnforceGLMinimums);
  if (GetUintFromSwitch(command_line, switches::kForceGpuMemAvailableMb,
                        &gpu_preferences.force_gpu_mem_available)) {
    gpu_preferences.force_gpu_mem_available *= 1024 * 1024;
  }
  if (GetUintFromSwitch(command_line, switches::kGpuProgramCacheSizeKb,
                        &gpu_preferences.gpu_program_cache_size)) {
    gpu_preferences.gpu_program_cache_size *= 1024;
  }
  gpu_preferences.disable_gpu_shader_disk_cache =
      command_line->HasSwitch(switches::kDisableGpuShaderDiskCache);
  gpu_preferences.enable_threaded_texture_mailboxes =
      command_line->HasSwitch(switches::kEnableThreadedTextureMailboxes);
  gpu_preferences.gl_shader_interm_output =
      command_line->HasSwitch(switches::kGLShaderIntermOutput);
  gpu_preferences.emulate_shader_precision =
      command_line->HasSwitch(switches::kEmulateShaderPrecision);
  gpu_preferences.enable_gpu_service_logging =
      command_line->HasSwitch(switches::kEnableGPUServiceLogging);
  gpu_preferences.enable_gpu_service_tracing =
      command_line->HasSwitch(switches::kEnableGPUServiceTracing);
  gpu_preferences.use_passthrough_cmd_decoder =
      gpu::gles2::UsePassthroughCommandDecoder(command_line);
  gpu_preferences.disable_gpu_driver_bug_workarounds =
      command_line->HasSwitch(switches::kDisableGpuDriverBugWorkarounds);
  gpu_preferences.ignore_gpu_blacklist =
      command_line->HasSwitch(switches::kIgnoreGpuBlacklist);
  gpu_preferences.enable_webgpu =
      command_line->HasSwitch(switches::kEnableUnsafeWebGPU);
  if (command_line->HasSwitch(switches::kUseVulkan)) {
    auto value = command_line->GetSwitchValueASCII(switches::kUseVulkan);
    if (value.empty() || value == switches::kVulkanImplementationNameNative) {
      gpu_preferences.use_vulkan = VulkanImplementationName::kForcedNative;
    } else if (value == switches::kVulkanImplementationNameSwiftshader) {
      gpu_preferences.use_vulkan = VulkanImplementationName::kSwiftshader;
    } else {
      gpu_preferences.use_vulkan = VulkanImplementationName::kNone;
    }
  }
  gpu_preferences.disable_vulkan_surface =
      command_line->HasSwitch(switches::kDisableVulkanSurface);
  if (command_line->HasSwitch(switches::kGrContextType)) {
    auto value = command_line->GetSwitchValueASCII(switches::kGrContextType);
    if (value == switches::kGrContextTypeGL) {
      gpu_preferences.gr_context_type = GrContextType::kGL;
    } else if (value == switches::kGrContextTypeVulkan) {
      gpu_preferences.gr_context_type = GrContextType::kVulkan;
    } else if (value == switches::kGrContextTypeMetal) {
#if defined(OS_MACOSX)
      DCHECK(base::FeatureList::IsEnabled(features::kMetal))
          << "GrContextType is Metal, but Metal is not enabled.";
      gpu_preferences.gr_context_type = GrContextType::kMetal;
#endif
#if BUILDFLAG(SKIA_USE_DAWN)
    } else if (value == switches::kGrContextTypeDawn) {
      gpu_preferences.gr_context_type = GrContextType::kDawn;
#endif
    } else {
      NOTREACHED() << "Invalid GrContextType.";
      gpu_preferences.gr_context_type = GrContextType::kGL;
    }
  } else {
#if defined(OS_MACOSX)
    gpu_preferences.gr_context_type =
        base::FeatureList::IsEnabled(features::kMetal) ?
            GrContextType::kMetal :
            GrContextType::kGL;
#else
    if (base::FeatureList::IsEnabled(features::kVulkan)) {
      gpu_preferences.gr_context_type = GrContextType::kVulkan;
    } else {
      gpu_preferences.gr_context_type = GrContextType::kGL;
    }
#endif
  }
  if (gpu_preferences.gr_context_type == GrContextType::kVulkan &&
      gpu_preferences.use_vulkan == gpu::VulkanImplementationName::kNone) {
    // If gpu_preferences.use_vulkan is not set from --use-vulkan, the native
    // vulkan implementation will be used by default.
    gpu_preferences.use_vulkan = gpu::VulkanImplementationName::kNative;
  }
  return gpu_preferences;
}

}  // namespace gles2
}  // namespace gpu
