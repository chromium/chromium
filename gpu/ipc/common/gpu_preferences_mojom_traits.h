// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_PREFERENCES_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_GPU_PREFERENCES_MOJOM_TRAITS_H_

#include <vector>

#include "base/notreached.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_preferences.mojom-shared.h"

#if BUILDFLAG(IS_OZONE)
#include "base/message_loop/message_pump_type.h"
#include "mojo/public/cpp/base/message_pump_type_mojom_traits.h"
#endif

namespace mojo {

template <>
struct GPU_EXPORT EnumTraits<gpu::mojom::GrContextType, gpu::GrContextType> {
  static gpu::mojom::GrContextType ToMojom(gpu::GrContextType input) {
    switch (input) {
      case gpu::GrContextType::kNone:
        return gpu::mojom::GrContextType::kNone;
      case gpu::GrContextType::kGL:
        return gpu::mojom::GrContextType::kGL;
      case gpu::GrContextType::kVulkan:
        return gpu::mojom::GrContextType::kVulkan;
      case gpu::GrContextType::kGraphiteDawn:
        return gpu::mojom::GrContextType::kGraphiteDawn;
      case gpu::GrContextType::kGraphiteMetal:
        return gpu::mojom::GrContextType::kGraphiteMetal;
    }
    NOTREACHED_IN_MIGRATION();
    return gpu::mojom::GrContextType::kGL;
  }
  static bool FromMojom(gpu::mojom::GrContextType input,
                        gpu::GrContextType* out) {
    switch (input) {
      case gpu::mojom::GrContextType::kNone:
        *out = gpu::GrContextType::kNone;
        return true;
      case gpu::mojom::GrContextType::kGL:
        *out = gpu::GrContextType::kGL;
        return true;
      case gpu::mojom::GrContextType::kVulkan:
        *out = gpu::GrContextType::kVulkan;
        return true;
      case gpu::mojom::GrContextType::kGraphiteDawn:
        *out = gpu::GrContextType::kGraphiteDawn;
        return true;
      case gpu::mojom::GrContextType::kGraphiteMetal:
        *out = gpu::GrContextType::kGraphiteMetal;
        return true;
    }
    return false;
  }
};

template <>
struct GPU_EXPORT EnumTraits<gpu::mojom::VulkanImplementationName,
                             gpu::VulkanImplementationName> {
  static gpu::mojom::VulkanImplementationName ToMojom(
      gpu::VulkanImplementationName input) {
    switch (input) {
      case gpu::VulkanImplementationName::kNone:
        return gpu::mojom::VulkanImplementationName::kNone;
      case gpu::VulkanImplementationName::kNative:
        return gpu::mojom::VulkanImplementationName::kNative;
      case gpu::VulkanImplementationName::kForcedNative:
        return gpu::mojom::VulkanImplementationName::kForcedNative;
      case gpu::VulkanImplementationName::kSwiftshader:
        return gpu::mojom::VulkanImplementationName::kSwiftshader;
    }
    NOTREACHED_IN_MIGRATION();
    return gpu::mojom::VulkanImplementationName::kNone;
  }
  static bool FromMojom(gpu::mojom::VulkanImplementationName input,
                        gpu::VulkanImplementationName* out) {
    switch (input) {
      case gpu::mojom::VulkanImplementationName::kNone:
        *out = gpu::VulkanImplementationName::kNone;
        return true;
      case gpu::mojom::VulkanImplementationName::kNative:
        *out = gpu::VulkanImplementationName::kNative;
        return true;
      case gpu::mojom::VulkanImplementationName::kForcedNative:
        *out = gpu::VulkanImplementationName::kForcedNative;
        return true;
      case gpu::mojom::VulkanImplementationName::kSwiftshader:
        *out = gpu::VulkanImplementationName::kSwiftshader;
        return true;
    }
    return false;
  }
};

template <>
struct GPU_EXPORT
    EnumTraits<gpu::mojom::WebGPUAdapterName, gpu::WebGPUAdapterName> {
  static gpu::mojom::WebGPUAdapterName ToMojom(gpu::WebGPUAdapterName input) {
    switch (input) {
      case gpu::WebGPUAdapterName::kDefault:
        return gpu::mojom::WebGPUAdapterName::kDefault;
      case gpu::WebGPUAdapterName::kD3D11:
        return gpu::mojom::WebGPUAdapterName::kD3D11;
      case gpu::WebGPUAdapterName::kOpenGLES:
        return gpu::mojom::WebGPUAdapterName::kOpenGLES;
      case gpu::WebGPUAdapterName::kSwiftShader:
        return gpu::mojom::WebGPUAdapterName::kSwiftShader;
    }
    NOTREACHED_IN_MIGRATION();
    return gpu::mojom::WebGPUAdapterName::kDefault;
  }
  static bool FromMojom(gpu::mojom::WebGPUAdapterName input,
                        gpu::WebGPUAdapterName* out) {
    switch (input) {
      case gpu::mojom::WebGPUAdapterName::kDefault:
        *out = gpu::WebGPUAdapterName::kDefault;
        return true;
      case gpu::mojom::WebGPUAdapterName::kD3D11:
        *out = gpu::WebGPUAdapterName::kD3D11;
        return true;
      case gpu::mojom::WebGPUAdapterName::kOpenGLES:
        *out = gpu::WebGPUAdapterName::kOpenGLES;
        return true;
      case gpu::mojom::WebGPUAdapterName::kSwiftShader:
        *out = gpu::WebGPUAdapterName::kSwiftShader;
        return true;
    }
    return false;
  }
};

template <>
struct GPU_EXPORT
    EnumTraits<gpu::mojom::WebGPUPowerPreference, gpu::WebGPUPowerPreference> {
  static gpu::mojom::WebGPUPowerPreference ToMojom(
      gpu::WebGPUPowerPreference input) {
    switch (input) {
      case gpu::WebGPUPowerPreference::kNone:
        return gpu::mojom::WebGPUPowerPreference::kNone;
      case gpu::WebGPUPowerPreference::kDefaultLowPower:
        return gpu::mojom::WebGPUPowerPreference::kDefaultLowPower;
      case gpu::WebGPUPowerPreference::kDefaultHighPerformance:
        return gpu::mojom::WebGPUPowerPreference::kDefaultHighPerformance;
      case gpu::WebGPUPowerPreference::kForceLowPower:
        return gpu::mojom::WebGPUPowerPreference::kForceLowPower;
      case gpu::WebGPUPowerPreference::kForceHighPerformance:
        return gpu::mojom::WebGPUPowerPreference::kForceHighPerformance;
    }
    NOTREACHED_IN_MIGRATION();
    return gpu::mojom::WebGPUPowerPreference::kNone;
  }

  static bool FromMojom(gpu::mojom::WebGPUPowerPreference input,
                        gpu::WebGPUPowerPreference* out) {
    switch (input) {
      case gpu::mojom::WebGPUPowerPreference::kNone:
        *out = gpu::WebGPUPowerPreference::kNone;
        return true;
      case gpu::mojom::WebGPUPowerPreference::kDefaultLowPower:
        *out = gpu::WebGPUPowerPreference::kDefaultLowPower;
        return true;
      case gpu::mojom::WebGPUPowerPreference::kDefaultHighPerformance:
        *out = gpu::WebGPUPowerPreference::kDefaultHighPerformance;
        return true;
      case gpu::mojom::WebGPUPowerPreference::kForceLowPower:
        *out = gpu::WebGPUPowerPreference::kForceLowPower;
        return true;
      case gpu::mojom::WebGPUPowerPreference::kForceHighPerformance:
        *out = gpu::WebGPUPowerPreference::kForceHighPerformance;
        return true;
    }
    return false;
  }
};

template <>
struct GPU_EXPORT EnumTraits<gpu::mojom::DawnBackendValidationLevel,
                             gpu::DawnBackendValidationLevel> {
  static gpu::mojom::DawnBackendValidationLevel ToMojom(
      gpu::DawnBackendValidationLevel input) {
    switch (input) {
      case gpu::DawnBackendValidationLevel::kDisabled:
        return gpu::mojom::DawnBackendValidationLevel::kDisabled;
      case gpu::DawnBackendValidationLevel::kPartial:
        return gpu::mojom::DawnBackendValidationLevel::kPartial;
      case gpu::DawnBackendValidationLevel::kFull:
        return gpu::mojom::DawnBackendValidationLevel::kFull;
    }
    NOTREACHED_IN_MIGRATION();
    return gpu::mojom::DawnBackendValidationLevel::kDisabled;
  }
  static bool FromMojom(gpu::mojom::DawnBackendValidationLevel input,
                        gpu::DawnBackendValidationLevel* out) {
    switch (input) {
      case gpu::mojom::DawnBackendValidationLevel::kDisabled:
        *out = gpu::DawnBackendValidationLevel::kDisabled;
        return true;
      case gpu::mojom::DawnBackendValidationLevel::kPartial:
        *out = gpu::DawnBackendValidationLevel::kPartial;
        return true;
      case gpu::mojom::DawnBackendValidationLevel::kFull:
        *out = gpu::DawnBackendValidationLevel::kFull;
        return true;
    }
    return false;
  }
};

template <>
struct GPU_EXPORT
    StructTraits<gpu::mojom::GpuPreferencesDataView, gpu::GpuPreferences> {
  static bool Read(gpu::mojom::GpuPreferencesDataView prefs,
                   gpu::GpuPreferences* out) {
    out->disable_accelerated_video_decode =
        prefs.disable_accelerated_video_decode();
    out->disable_accelerated_video_encode =
        prefs.disable_accelerated_video_encode();
    out->gpu_startup_dialog = prefs.gpu_startup_dialog();
    out->disable_gpu_watchdog = prefs.disable_gpu_watchdog();
    out->gpu_sandbox_start_early = prefs.gpu_sandbox_start_early();
    out->enable_low_latency_dxva = prefs.enable_low_latency_dxva();
    out->enable_zero_copy_dxgi_video = prefs.enable_zero_copy_dxgi_video();
    out->enable_nv12_dxgi_video = prefs.enable_nv12_dxgi_video();
    out->disable_software_rasterizer = prefs.disable_software_rasterizer();
    out->log_gpu_control_list_decisions =
        prefs.log_gpu_control_list_decisions();
    out->compile_shader_always_succeeds =
        prefs.compile_shader_always_succeeds();
    out->disable_gl_error_limit = prefs.disable_gl_error_limit();
    out->disable_glsl_translator = prefs.disable_glsl_translator();
    out->disable_shader_name_hashing = prefs.disable_shader_name_hashing();
    out->enable_gpu_command_logging = prefs.enable_gpu_command_logging();
    out->enable_gpu_debugging = prefs.enable_gpu_debugging();
    out->enable_gpu_service_logging_gpu =
        prefs.enable_gpu_service_logging_gpu();
    out->enable_gpu_driver_debug_logging =
        prefs.enable_gpu_driver_debug_logging();
    out->disable_gpu_program_cache = prefs.disable_gpu_program_cache();
    out->enforce_gl_minimums = prefs.enforce_gl_minimums();
    out->force_gpu_mem_available_bytes = prefs.force_gpu_mem_available_bytes();
    out->force_gpu_mem_discardable_limit_bytes =
        prefs.force_gpu_mem_discardable_limit_bytes();
    out->force_max_texture_size = prefs.force_max_texture_size();
    out->gpu_program_cache_size = prefs.gpu_program_cache_size();
    out->disable_gpu_shader_disk_cache = prefs.disable_gpu_shader_disk_cache();
    out->enable_threaded_texture_mailboxes =
        prefs.enable_threaded_texture_mailboxes();
    out->gl_shader_interm_output = prefs.gl_shader_interm_output();
    out->enable_android_surface_control =
        prefs.enable_android_surface_control();
    out->enable_gpu_service_logging = prefs.enable_gpu_service_logging();
    out->enable_gpu_service_tracing = prefs.enable_gpu_service_tracing();
    out->use_passthrough_cmd_decoder = prefs.use_passthrough_cmd_decoder();

    out->ignore_gpu_blocklist = prefs.ignore_gpu_blocklist();
    out->watchdog_starts_backgrounded = prefs.watchdog_starts_backgrounded();
    if (!prefs.ReadGrContextType(&out->gr_context_type)) {
      return false;
    }
    if (!prefs.ReadUseVulkan(&out->use_vulkan)) {
      return false;
    }
    out->enable_vulkan_protected_memory =
        prefs.enable_vulkan_protected_memory();
    out->disable_vulkan_surface = prefs.disable_vulkan_surface();
    out->disable_vulkan_fallback_to_gl_for_testing =
        prefs.disable_vulkan_fallback_to_gl_for_testing();
    out->vulkan_heap_memory_limit = prefs.vulkan_heap_memory_limit();
    out->vulkan_sync_cpu_memory_limit = prefs.vulkan_sync_cpu_memory_limit();
    out->enable_gpu_benchmarking_extension =
        prefs.enable_gpu_benchmarking_extension();
    out->enable_webgpu = prefs.enable_webgpu();
    out->enable_unsafe_webgpu = prefs.enable_unsafe_webgpu();
    out->enable_webgpu_developer_features =
        prefs.enable_webgpu_developer_features();
    out->enable_webgpu_experimental_features =
        prefs.enable_webgpu_experimental_features();
    if (!prefs.ReadUseWebgpuAdapter(&out->use_webgpu_adapter))
      return false;
    if (!prefs.ReadUseWebgpuPowerPreference(
            &out->use_webgpu_power_preference)) {
      return false;
    }
    out->force_webgpu_compat = prefs.force_webgpu_compat();
    if (!prefs.ReadEnableDawnBackendValidation(
            &out->enable_dawn_backend_validation))
      return false;
    if (!prefs.ReadEnabledDawnFeaturesList(&out->enabled_dawn_features_list))
      return false;
    if (!prefs.ReadDisabledDawnFeaturesList(&out->disabled_dawn_features_list))
      return false;

    out->enable_perf_data_collection = prefs.enable_perf_data_collection();

#if BUILDFLAG(IS_OZONE)
    if (!prefs.ReadMessagePumpType(&out->message_pump_type))
      return false;
#endif

    out->enable_native_gpu_memory_buffers =
        prefs.enable_native_gpu_memory_buffers();

    out->force_separate_egl_display_for_webgl_testing =
        prefs.force_separate_egl_display_for_webgl_testing();

    return true;
  }

  static bool disable_accelerated_video_decode(
      const gpu::GpuPreferences& prefs) {
    return prefs.disable_accelerated_video_decode;
  }
  static bool disable_accelerated_video_encode(
      const gpu::GpuPreferences& prefs) {
    return prefs.disable_accelerated_video_encode;
  }
  static bool gpu_startup_dialog(const gpu::GpuPreferences& prefs) {
    return prefs.gpu_startup_dialog;
  }
  static bool disable_gpu_watchdog(const gpu::GpuPreferences& prefs) {
    return prefs.disable_gpu_watchdog;
  }
  static bool gpu_sandbox_start_early(const gpu::GpuPreferences& prefs) {
    return prefs.gpu_sandbox_start_early;
  }
  static bool enable_low_latency_dxva(const gpu::GpuPreferences& prefs) {
    return prefs.enable_low_latency_dxva;
  }
  static bool enable_zero_copy_dxgi_video(const gpu::GpuPreferences& prefs) {
    return prefs.enable_zero_copy_dxgi_video;
  }
  static bool enable_nv12_dxgi_video(const gpu::GpuPreferences& prefs) {
    return prefs.enable_nv12_dxgi_video;
  }
  static bool disable_software_rasterizer(const gpu::GpuPreferences& prefs) {
    return prefs.disable_software_rasterizer;
  }
  static bool log_gpu_control_list_decisions(const gpu::GpuPreferences& prefs) {
    return prefs.log_gpu_control_list_decisions;
  }
  static bool compile_shader_always_succeeds(const gpu::GpuPreferences& prefs) {
    return prefs.compile_shader_always_succeeds;
  }
  static bool disable_gl_error_limit(const gpu::GpuPreferences& prefs) {
    return prefs.disable_gl_error_limit;
  }
  static bool disable_glsl_translator(const gpu::GpuPreferences& prefs) {
    return prefs.disable_glsl_translator;
  }
  static bool disable_shader_name_hashing(const gpu::GpuPreferences& prefs) {
    return prefs.disable_shader_name_hashing;
  }
  static bool enable_gpu_command_logging(const gpu::GpuPreferences& prefs) {
    return prefs.enable_gpu_command_logging;
  }
  static bool enable_gpu_debugging(const gpu::GpuPreferences& prefs) {
    return prefs.enable_gpu_debugging;
  }
  static bool enable_gpu_service_logging_gpu(const gpu::GpuPreferences& prefs) {
    return prefs.enable_gpu_service_logging_gpu;
  }
  static bool enable_gpu_driver_debug_logging(
      const gpu::GpuPreferences& prefs) {
    return prefs.enable_gpu_driver_debug_logging;
  }
  static bool disable_gpu_program_cache(const gpu::GpuPreferences& prefs) {
    return prefs.disable_gpu_program_cache;
  }
  static bool enforce_gl_minimums(const gpu::GpuPreferences& prefs) {
    return prefs.enforce_gl_minimums;
  }
  static uint32_t force_gpu_mem_available_bytes(
      const gpu::GpuPreferences& prefs) {
    return prefs.force_gpu_mem_available_bytes;
  }
  static uint32_t force_gpu_mem_discardable_limit_bytes(
      const gpu::GpuPreferences& prefs) {
    return prefs.force_gpu_mem_discardable_limit_bytes;
  }
  static uint32_t force_max_texture_size(const gpu::GpuPreferences& prefs) {
    return prefs.force_max_texture_size;
  }
  static uint32_t gpu_program_cache_size(const gpu::GpuPreferences& prefs) {
    return prefs.gpu_program_cache_size;
  }
  static bool disable_gpu_shader_disk_cache(const gpu::GpuPreferences& prefs) {
    return prefs.disable_gpu_shader_disk_cache;
  }
  static bool enable_threaded_texture_mailboxes(
      const gpu::GpuPreferences& prefs) {
    return prefs.enable_threaded_texture_mailboxes;
  }
  static bool gl_shader_interm_output(const gpu::GpuPreferences& prefs) {
    return prefs.gl_shader_interm_output;
  }
  static bool enable_android_surface_control(const gpu::GpuPreferences& prefs) {
    return prefs.enable_android_surface_control;
  }
  static bool enable_gpu_service_logging(const gpu::GpuPreferences& prefs) {
    return prefs.enable_gpu_service_logging;
  }
  static bool enable_gpu_service_tracing(const gpu::GpuPreferences& prefs) {
    return prefs.enable_gpu_service_tracing;
  }
  static bool use_passthrough_cmd_decoder(const gpu::GpuPreferences& prefs) {
    return prefs.use_passthrough_cmd_decoder;
  }
  static bool ignore_gpu_blocklist(const gpu::GpuPreferences& prefs) {
    return prefs.ignore_gpu_blocklist;
  }
  static bool watchdog_starts_backgrounded(const gpu::GpuPreferences& prefs) {
    return prefs.watchdog_starts_backgrounded;
  }
  static gpu::GrContextType gr_context_type(const gpu::GpuPreferences& prefs) {
    return prefs.gr_context_type;
  }
  static gpu::VulkanImplementationName use_vulkan(
      const gpu::GpuPreferences& prefs) {
    return prefs.use_vulkan;
  }
  static bool enable_vulkan_protected_memory(const gpu::GpuPreferences& prefs) {
    return prefs.enable_vulkan_protected_memory;
  }
  static bool disable_vulkan_surface(const gpu::GpuPreferences& prefs) {
    return prefs.disable_vulkan_surface;
  }
  static bool disable_vulkan_fallback_to_gl_for_testing(
      const gpu::GpuPreferences& prefs) {
    return prefs.disable_vulkan_fallback_to_gl_for_testing;
  }
  static uint32_t vulkan_heap_memory_limit(const gpu::GpuPreferences& prefs) {
    return prefs.vulkan_heap_memory_limit;
  }
  static uint32_t vulkan_sync_cpu_memory_limit(
      const gpu::GpuPreferences& prefs) {
    return prefs.vulkan_sync_cpu_memory_limit;
  }
  static bool enable_gpu_benchmarking_extension(
      const gpu::GpuPreferences& prefs) {
    return prefs.enable_gpu_benchmarking_extension;
  }
  static bool enable_webgpu(const gpu::GpuPreferences& prefs) {
    return prefs.enable_webgpu;
  }
  static bool enable_unsafe_webgpu(const gpu::GpuPreferences& prefs) {
    return prefs.enable_unsafe_webgpu;
  }
  static bool enable_webgpu_developer_features(
      const gpu::GpuPreferences& prefs) {
    return prefs.enable_webgpu_developer_features;
  }
  static bool enable_webgpu_experimental_features(
      const gpu::GpuPreferences& prefs) {
    return prefs.enable_webgpu_experimental_features;
  }
  static gpu::WebGPUAdapterName use_webgpu_adapter(
      const gpu::GpuPreferences& prefs) {
    return prefs.use_webgpu_adapter;
  }
  static gpu::WebGPUPowerPreference use_webgpu_power_preference(
      const gpu::GpuPreferences& prefs) {
    return prefs.use_webgpu_power_preference;
  }
  static bool force_webgpu_compat(const gpu::GpuPreferences& prefs) {
    return prefs.force_webgpu_compat;
  }
  static gpu::DawnBackendValidationLevel enable_dawn_backend_validation(
      const gpu::GpuPreferences& prefs) {
    return prefs.enable_dawn_backend_validation;
  }
  static const std::vector<std::string>& enabled_dawn_features_list(
      const gpu::GpuPreferences& prefs) {
    return prefs.enabled_dawn_features_list;
  }
  static const std::vector<std::string>& disabled_dawn_features_list(
      const gpu::GpuPreferences& prefs) {
    return prefs.disabled_dawn_features_list;
  }
  static bool enable_perf_data_collection(const gpu::GpuPreferences& prefs) {
    return prefs.enable_perf_data_collection;
  }
#if BUILDFLAG(IS_OZONE)
  static base::MessagePumpType message_pump_type(
      const gpu::GpuPreferences& prefs) {
    return prefs.message_pump_type;
  }
#endif
  static bool enable_native_gpu_memory_buffers(
      const gpu::GpuPreferences& prefs) {
    return prefs.enable_native_gpu_memory_buffers;
  }
  static bool force_separate_egl_display_for_webgl_testing(
      const gpu::GpuPreferences& prefs) {
    return prefs.force_separate_egl_display_for_webgl_testing;
  }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_GPU_PREFERENCES_MOJOM_TRAITS_H_
