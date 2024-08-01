// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/service_utils.h"

#include <string>
#include <string_view>

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
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gl_surface_egl.h"

namespace gpu {
namespace gles2 {

namespace {

bool GetUintFromSwitch(const base::CommandLine* command_line,
                       std::string_view switch_string,
                       uint32_t* value) {
  if (!command_line->HasSwitch(switch_string)) {
    return false;
  }
  std::string switch_value(command_line->GetSwitchValueASCII(switch_string));
  return base::StringToUint(switch_value, value);
}

// Parse the value of --use-vulkan from the command line. If unspecified and
// features::kVulkan is enabled (GrContext is going to use vulkan), default to
// the native implementation.
VulkanImplementationName ParseVulkanImplementationName(
    const base::CommandLine* command_line) {
#if BUILDFLAG(IS_ANDROID)
  if (command_line->HasSwitch(switches::kWebViewDrawFunctorUsesVulkan)) {
    return VulkanImplementationName::kForcedNative;
  }
#endif

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
}

WebGPUAdapterName ParseWebGPUAdapterName(
    const base::CommandLine* command_line) {
  if (command_line->HasSwitch(switches::kUseWebGPUAdapter)) {
    auto value = command_line->GetSwitchValueASCII(switches::kUseWebGPUAdapter);

    static const struct {
      const char* name;
      WebGPUAdapterName value;
    } kAdapterNames[] = {
        {"", WebGPUAdapterName::kDefault},
        {"default", WebGPUAdapterName::kDefault},
        {"d3d11", WebGPUAdapterName::kD3D11},
        {"opengles", WebGPUAdapterName::kOpenGLES},
        {"swiftshader", WebGPUAdapterName::kSwiftShader},
    };

    for (const auto& adapter_name : kAdapterNames) {
      if (value == adapter_name.name) {
        return adapter_name.value;
      }
    }

    DLOG(ERROR) << "Invalid switch " << switches::kUseWebGPUAdapter << "="
                << value << ".";
  }
  return WebGPUAdapterName::kDefault;
}

WebGPUPowerPreference ParseWebGPUPowerPreference(
    const base::CommandLine* command_line) {
  if (command_line->HasSwitch(switches::kUseWebGPUPowerPreference)) {
    auto value =
        command_line->GetSwitchValueASCII(switches::kUseWebGPUPowerPreference);
    if (value.empty()) {
      return WebGPUPowerPreference::kNone;
    } else if (value == "none") {
      return WebGPUPowerPreference::kNone;
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
  return WebGPUPowerPreference::kNone;
}

}  // namespace

gl::GLContextAttribs GenerateGLContextAttribsForDecoder(
    const ContextCreationAttribs& attribs_helper,
    const ContextGroup* context_group) {
  gl::GLContextAttribs attribs;
  attribs.gpu_preference = attribs_helper.gpu_preference;
  if (context_group->use_passthrough_cmd_decoder()) {
    attribs.bind_generates_resource = attribs_helper.bind_generates_resource;
    attribs.webgl_compatibility_context =
        IsWebGLContextType(attribs_helper.context_type);

    // Always use the global texture and semaphore share group for the
    // passthrough command decoder
    attribs.global_texture_share_group = true;
    attribs.global_semaphore_share_group = true;

    attribs.robust_resource_initialization = true;
    attribs.robust_buffer_access = true;
    attribs.allow_client_arrays = false;

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

gl::GLContextAttribs GenerateGLContextAttribsForCompositor(
    bool use_passthrough_cmd_decoder) {
  gl::GLContextAttribs attribs;
  if (use_passthrough_cmd_decoder) {
    attribs.bind_generates_resource = false;

    // Always use the global texture and semaphore share group for the
    // passthrough command decoder
    attribs.global_texture_share_group = true;
    attribs.global_semaphore_share_group = true;

    // Disable resource initialization and buffer bounds checks for trusted
    // contexts.
    attribs.robust_resource_initialization = false;
    attribs.robust_buffer_access = false;
    attribs.allow_client_arrays = true;
  }

  bool force_es2_context = gl::GetGlWorkarounds().disable_es3gl_context;
  if (features::UseGles2ForOopR() && use_passthrough_cmd_decoder) {
    force_es2_context = true;
  }

  attribs.client_major_es_version = force_es2_context ? 2 : 3;
  attribs.client_minor_es_version = 0;

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
  gpu_preferences.enable_webgpu_developer_features =
      command_line->HasSwitch(switches::kEnableWebGPUDeveloperFeatures);
  gpu_preferences.use_webgpu_adapter = ParseWebGPUAdapterName(command_line);
  gpu_preferences.use_webgpu_power_preference =
      ParseWebGPUPowerPreference(command_line);
  gpu_preferences.force_webgpu_compat =
      command_line->HasSwitch(switches::kForceWebGPUCompat);
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
  // ParseGrContextType checks Vulkan setting as well, so only parse Vulkan
  // implementation name if gr_context_type is kVulkan.
  gpu_preferences.use_vulkan =
      gpu_preferences.gr_context_type == GrContextType::kVulkan
          ? ParseVulkanImplementationName(command_line)
          : VulkanImplementationName::kNone;

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
  if (features::IsSkiaGraphiteEnabled(command_line)) {
    [[maybe_unused]] auto value =
        command_line->GetSwitchValueASCII(switches::kSkiaGraphiteBackend);
#if BUILDFLAG(SKIA_USE_DAWN)
    if (value.empty() ||
        base::StartsWith(value, switches::kSkiaGraphiteBackendDawn)) {
      return GrContextType::kGraphiteDawn;
    }
#endif  // BUILDFLAG(SKIA_USE_DAWN)
#if BUILDFLAG(SKIA_USE_METAL)
    if (value == switches::kSkiaGraphiteBackendMetal) {
      return GrContextType::kGraphiteMetal;
    }
#endif  // BUILDFLAG(SKIA_USE_METAL)
    LOG(ERROR) << "Skia Graphite backend = \"" << value
               << "\" not found - falling back to Ganesh!";
  }
  if (features::IsUsingVulkan()) {
    return GrContextType::kVulkan;
  }
  return GrContextType::kGL;
}

bool MSAAIsSlow(const GpuDriverBugWorkarounds& workarounds) {
  // Only query the kEnableMSAAOnNewIntelGPUs feature flag if the host device
  // is affected by the experiment (i.e. is a new Intel GPU).
  // This is to avoid activating the experiment on hosts that are irrelevant
  // to the study in order to boost statistical power.
  bool affected_by_experiment =
      workarounds.msaa_is_slow && !workarounds.msaa_is_slow_2;

  return affected_by_experiment ? !base::FeatureList::IsEnabled(
                                      features::kEnableMSAAOnNewIntelGPUs)
                                : workarounds.msaa_is_slow;
}

}  // namespace gles2

#if BUILDFLAG(IS_MAC)
uint32_t GetTextureTargetForIOSurfaces() {
  // On MacOS, the default texture target for native GpuMemoryBuffers is
  // GL_TEXTURE_RECTANGLE_ARB. This is due to CGL's requirements for creating
  // a GL surface. However, when ANGLE is used on top of SwiftShader or Metal,
  // it's necessary to use GL_TEXTURE_2D instead.
  // TODO(crbug.com/40676774): The proper behavior is to check the config
  // parameter set by the EGL_ANGLE_iosurface_client_buffer extension
  if (gl::GetGLImplementation() == gl::kGLImplementationEGLANGLE &&
      (gl::GetANGLEImplementation() == gl::ANGLEImplementation::kSwiftShader ||
       gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal)) {
    return GL_TEXTURE_2D;
  }
  return GL_TEXTURE_RECTANGLE_ARB;
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace gpu
