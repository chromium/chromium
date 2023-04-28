// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/service_utils.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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

    // Always use the global texture and semaphore share group for the
    // passthrough command decoder
    attribs.global_texture_share_group = true;
    attribs.global_semaphore_share_group = true;

    attribs.robust_resource_initialization = true;
    attribs.robust_buffer_access = true;

    // Request a specific context version instead of always 3.0
    if (IsWebGL2OrES3ContextType(attribs_helper.context_type)) {
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

  if (gl::GetGlWorkarounds().disable_es3gl_context) {
    // Forcefully disable ES3 contexts
    attribs.client_major_es_version = 2;
    attribs.client_minor_es_version = 0;
  }

  if (IsES31ForTestingContextType(attribs_helper.context_type)) {
    // Forcefully disable ES 3.1 contexts. Tests create contexts by initializing
    // the attributes directly.
    attribs.client_major_es_version = 2;
    attribs.client_minor_es_version = 0;
  }

  return attribs;
}

bool UsePassthroughCommandDecoder(const base::CommandLine* command_line) {
  return gl::UsePassthroughCommandDecoder(command_line);
}

bool PassthroughCommandDecoderSupported() {
  return gl::PassthroughCommandDecoderSupported();
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
                        &gpu_preferences.force_gpu_mem_available_bytes)) {
    gpu_preferences.force_gpu_mem_available_bytes *= 1024 * 1024;
  }
  if (GetUintFromSwitch(
          command_line, switches::kForceGpuMemDiscardableLimitMb,
          &gpu_preferences.force_gpu_mem_discardable_limit_bytes)) {
    gpu_preferences.force_gpu_mem_discardable_limit_bytes *= 1024 * 1024;
  }
  GetUintFromSwitch(command_line, switches::kForceMaxTextureSize,
                    &gpu_preferences.force_max_texture_size);
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
  gpu_preferences.enable_gpu_service_logging =
      command_line->HasSwitch(switches::kEnableGPUServiceLogging);
  gpu_preferences.enable_gpu_service_tracing =
      command_line->HasSwitch(switches::kEnableGPUServiceTracing);
  gpu_preferences.use_passthrough_cmd_decoder =
      gpu::gles2::UsePassthroughCommandDecoder(command_line);
  gpu_preferences.ignore_gpu_blocklist =
      command_line->HasSwitch(switches::kIgnoreGpuBlocklist);
  gpu_preferences.enable_webgpu =
      command_line->HasSwitch(switches::kEnableUnsafeWebGPU) ||
      base::FeatureList::IsEnabled(features::kWebGPUService);
  gpu_preferences.enable_unsafe_webgpu =
      command_line->HasSwitch(switches::kEnableUnsafeWebGPU);
  gpu_preferences.use_webgpu_adapter = ParseWebGPUAdapterName(command_line);
  gpu_preferences.use_webgpu_power_preference =
      ParseWebGPUPowerPreference(command_line);
  if (command_line->HasSwitch(switches::kEnableDawnBackendValidation)) {
    auto value = command_line->GetSwitchValueASCII(
        switches::kEnableDawnBackendValidation);
    if (value.empty() || value == "full") {
      gpu_preferences.enable_dawn_backend_validation =
          DawnBackendValidationLevel::kFull;
    } else if (value == "partial") {
      gpu_preferences.enable_dawn_backend_validation =
          DawnBackendValidationLevel::kPartial;
    }
  }
  if (command_line->HasSwitch(switches::kEnableDawnFeatures)) {
    gpu_preferences.enabled_dawn_features_list = base::SplitString(
        command_line->GetSwitchValueASCII(switches::kEnableDawnFeatures), ",",
        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  }
  if (command_line->HasSwitch(switches::kDisableDawnFeatures)) {
    gpu_preferences.disabled_dawn_features_list = base::SplitString(
        command_line->GetSwitchValueASCII(switches::kDisableDawnFeatures), ",",
        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  }
  gpu_preferences.gr_context_type = ParseGrContextType(command_line);
  gpu_preferences.use_vulkan = ParseVulkanImplementationName(command_line);

#if BUILDFLAG(IS_FUCHSIA)
  // Vulkan Surface is not used on Fuchsia.
  gpu_preferences.disable_vulkan_surface = true;
#else
  gpu_preferences.disable_vulkan_surface =
      command_line->HasSwitch(switches::kDisableVulkanSurface);
#endif

  gpu_preferences.enable_gpu_blocked_time_metric =
      command_line->HasSwitch(switches::kEnableGpuBlockedTime);

  return gpu_preferences;
}

GrContextType ParseGrContextType(const base::CommandLine* command_line) {
#if BUILDFLAG(ENABLE_SKIA_GRAPHITE)
  if (base::FeatureList::IsEnabled(features::kSkiaGraphite)) {
    [[maybe_unused]] auto value =
        command_line->GetSwitchValueASCII(switches::kSkiaGraphiteBackend);
#if BUILDFLAG(SKIA_USE_DAWN)
    if (value.empty() || value == switches::kSkiaGraphiteBackendDawn) {
      return GrContextType::kGraphiteDawn;
    }
#endif  // BUILDFLAG(SKIA_USE_DAWN)
#if BUILDFLAG(SKIA_USE_METAL)
    if (value == switches::kSkiaGraphiteBackendMetal) {
      return GrContextType::kGraphiteMetal;
    }
#endif  // BUILDFLAG(SKIA_USE_METAL)
  }
#endif  // BUILDFLAG(ENABLE_SKIA_GRAPHITE)

  if (features::IsUsingVulkan()) {
    return GrContextType::kVulkan;
  }

  return GrContextType::kGL;
}

VulkanImplementationName ParseVulkanImplementationName(
    const base::CommandLine* command_line) {
#if BUILDFLAG(IS_ANDROID)
  if (command_line->HasSwitch(switches::kWebViewDrawFunctorUsesVulkan) &&
      base::FeatureList::IsEnabled(features::kWebViewVulkan)) {
    return VulkanImplementationName::kForcedNative;
  }
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // LACROS doesn't support Vulkan right now, to avoid LACROS picking up Linux
  // finch, kNone is returned for LACROS.
  // TODO(https://crbug.com/1155622): When LACROS is separated from Linux finch
  // config.
  return VulkanImplementationName::kNone;
#else
  if (command_line->HasSwitch(switches::kUseVulkan)) {
    auto value = command_line->GetSwitchValueASCII(switches::kUseVulkan);
    if (value.empty() || value == switches::kVulkanImplementationNameNative) {
      return VulkanImplementationName::kForcedNative;
    } else if (value == switches::kVulkanImplementationNameSwiftshader) {
      return VulkanImplementationName::kSwiftshader;
    }
  }

  if (features::IsUsingVulkan()) {
    // If the vulkan feature is enabled from command line, we will force to use
    // vulkan even if it is blocklisted.
    return base::FeatureList::GetInstance()->IsFeatureOverriddenFromCommandLine(
               features::kVulkan.name,
               base::FeatureList::OVERRIDE_ENABLE_FEATURE)
               ? VulkanImplementationName::kForcedNative
               : VulkanImplementationName::kNative;
  }

  // GrContext is not going to use Vulkan.
  return VulkanImplementationName::kNone;
#endif
}

WebGPUAdapterName ParseWebGPUAdapterName(
    const base::CommandLine* command_line) {
  if (command_line->HasSwitch(switches::kUseWebGPUAdapter)) {
    auto value = command_line->GetSwitchValueASCII(switches::kUseWebGPUAdapter);
    if (value.empty()) {
      return WebGPUAdapterName::kDefault;
    } else if (value == "compat") {
      return WebGPUAdapterName::kCompat;
    } else if (value == "swiftshader") {
      return WebGPUAdapterName::kSwiftShader;
    } else if (value == "default") {
      return WebGPUAdapterName::kDefault;
    } else {
      DLOG(ERROR) << "Invalid switch " << switches::kUseWebGPUAdapter << "="
                  << value << ".";
    }
  }
  return WebGPUAdapterName::kDefault;
}

WebGPUPowerPreference ParseWebGPUPowerPreference(
    const base::CommandLine* command_line) {
  if (command_line->HasSwitch(switches::kUseWebGPUPowerPreference)) {
    auto value =
        command_line->GetSwitchValueASCII(switches::kUseWebGPUPowerPreference);
    if (value.empty()) {
      return WebGPUPowerPreference::kDefaultLowPower;
    } else if (value == "default-low-power") {
      return WebGPUPowerPreference::kDefaultLowPower;
    } else if (value == "default-high-performance") {
      return WebGPUPowerPreference::kDefaultHighPerformance;
    } else if (value == "force-low-power") {
      return WebGPUPowerPreference::kForceLowPower;
    } else if (value == "force-high-performance") {
      return WebGPUPowerPreference::kForceHighPerformance;
    } else {
      DLOG(ERROR) << "Invalid switch " << switches::kUseWebGPUPowerPreference
                  << "=" << value << ".";
    }
  }
  return WebGPUPowerPreference::kDefaultLowPower;
}

}  // namespace gles2
}  // namespace gpu
