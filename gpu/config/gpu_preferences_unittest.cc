// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstring>

#include "base/command_line.h"
#include "base/message_loop/message_pump_type.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/common/gpu_preferences.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

namespace {

void CheckGpuPreferencesEqual(GpuPreferences left, GpuPreferences right) {
  EXPECT_EQ(left.disable_accelerated_video_decode,
            right.disable_accelerated_video_decode);
  EXPECT_EQ(left.disable_accelerated_video_encode,
            right.disable_accelerated_video_encode);
  EXPECT_EQ(left.gpu_startup_dialog, right.gpu_startup_dialog);
  EXPECT_EQ(left.disable_gpu_watchdog, right.disable_gpu_watchdog);
  EXPECT_EQ(left.gpu_sandbox_start_early, right.gpu_sandbox_start_early);
  EXPECT_EQ(left.enable_low_latency_dxva, right.enable_low_latency_dxva);
  EXPECT_EQ(left.enable_zero_copy_dxgi_video,
            right.enable_zero_copy_dxgi_video);
  EXPECT_EQ(left.enable_nv12_dxgi_video, right.enable_nv12_dxgi_video);
  EXPECT_EQ(left.disable_software_rasterizer,
            right.disable_software_rasterizer);
  EXPECT_EQ(left.log_gpu_control_list_decisions,
            right.log_gpu_control_list_decisions);
  EXPECT_EQ(left.compile_shader_always_succeeds,
            right.compile_shader_always_succeeds);
  EXPECT_EQ(left.disable_gl_error_limit, right.disable_gl_error_limit);
  EXPECT_EQ(left.disable_glsl_translator, right.disable_glsl_translator);
  EXPECT_EQ(left.disable_shader_name_hashing,
            right.disable_shader_name_hashing);
  EXPECT_EQ(left.enable_gpu_command_logging, right.enable_gpu_command_logging);
  EXPECT_EQ(left.enable_gpu_debugging, right.enable_gpu_debugging);
  EXPECT_EQ(left.enable_gpu_service_logging_gpu,
            right.enable_gpu_service_logging_gpu);
  EXPECT_EQ(left.enable_gpu_driver_debug_logging,
            right.enable_gpu_driver_debug_logging);
  EXPECT_EQ(left.disable_gpu_program_cache, right.disable_gpu_program_cache);
  EXPECT_EQ(left.enforce_gl_minimums, right.enforce_gl_minimums);
  EXPECT_EQ(left.force_gpu_mem_available_bytes,
            right.force_gpu_mem_available_bytes);
  EXPECT_EQ(left.force_gpu_mem_discardable_limit_bytes,
            right.force_gpu_mem_discardable_limit_bytes);
  EXPECT_EQ(left.gpu_program_cache_size, right.gpu_program_cache_size);
  EXPECT_EQ(left.disable_gpu_shader_disk_cache,
            right.disable_gpu_shader_disk_cache);
  EXPECT_EQ(left.enable_threaded_texture_mailboxes,
            right.enable_threaded_texture_mailboxes);
  EXPECT_EQ(left.gl_shader_interm_output, right.gl_shader_interm_output);
  EXPECT_EQ(left.enable_gpu_service_logging, right.enable_gpu_service_logging);
  EXPECT_EQ(left.enable_gpu_service_tracing, right.enable_gpu_service_tracing);
  EXPECT_EQ(left.use_passthrough_cmd_decoder,
            right.use_passthrough_cmd_decoder);
  EXPECT_EQ(left.ignore_gpu_blocklist, right.ignore_gpu_blocklist);
  EXPECT_EQ(left.watchdog_starts_backgrounded,
            right.watchdog_starts_backgrounded);
  EXPECT_EQ(left.gr_context_type, right.gr_context_type);
  EXPECT_EQ(left.use_vulkan, right.use_vulkan);
  EXPECT_EQ(left.enable_vulkan_protected_memory,
            right.enable_vulkan_protected_memory);
  EXPECT_EQ(left.vulkan_heap_memory_limit, right.vulkan_heap_memory_limit);
  EXPECT_EQ(left.vulkan_sync_cpu_memory_limit,
            right.vulkan_sync_cpu_memory_limit);
  EXPECT_EQ(left.enable_gpu_benchmarking_extension,
            right.enable_gpu_benchmarking_extension);
  EXPECT_EQ(left.enable_webgpu, right.enable_webgpu);
  EXPECT_EQ(left.enable_dawn_backend_validation,
            right.enable_dawn_backend_validation);
  EXPECT_EQ(left.enabled_dawn_features_list, right.enabled_dawn_features_list);
  EXPECT_EQ(left.disabled_dawn_features_list,
            right.disabled_dawn_features_list);
  EXPECT_EQ(left.enable_perf_data_collection,
            right.enable_perf_data_collection);
#if BUILDFLAG(IS_OZONE)
  EXPECT_EQ(left.message_pump_type, right.message_pump_type);
#endif
  EXPECT_EQ(left.enable_native_gpu_memory_buffers,
            right.enable_native_gpu_memory_buffers);
  EXPECT_EQ(left.force_separate_egl_display_for_webgl_testing,
            right.force_separate_egl_display_for_webgl_testing);
}

}  // namespace

TEST(GpuPreferencesTest, EncodeDecode) {
  {  // Testing default values.
    GpuPreferences input_prefs;
    GpuPreferences decoded_prefs;
    std::string encoded = input_prefs.ToSwitchValue();
    bool flag = decoded_prefs.FromSwitchValue(encoded);
    EXPECT_TRUE(flag);
    CheckGpuPreferencesEqual(input_prefs, decoded_prefs);
  }

  {  // Change all fields to non default values.
    GpuPreferences input_prefs;
    GpuPreferences decoded_prefs;

    GpuPreferences default_prefs;
    mojom::GpuPreferences prefs_mojom;

#define GPU_PREFERENCES_FIELD(name, value)         \
  input_prefs.name = value;                        \
  EXPECT_NE(default_prefs.name, input_prefs.name); \
  prefs_mojom.name = value;                        \
  EXPECT_EQ(input_prefs.name, prefs_mojom.name);

#define GPU_PREFERENCES_FIELD_ENUM(name, value, mojom_value) \
  input_prefs.name = value;                                  \
  EXPECT_NE(default_prefs.name, input_prefs.name);           \
  prefs_mojom.name = mojom_value;                            \
  EXPECT_EQ(static_cast<uint32_t>(input_prefs.name),         \
            static_cast<uint32_t>(prefs_mojom.name));

    GPU_PREFERENCES_FIELD(disable_accelerated_video_decode, true)
    GPU_PREFERENCES_FIELD(disable_accelerated_video_encode, true)
    GPU_PREFERENCES_FIELD(gpu_startup_dialog, true)
    GPU_PREFERENCES_FIELD(disable_gpu_watchdog, true)
    GPU_PREFERENCES_FIELD(gpu_sandbox_start_early, true)
    GPU_PREFERENCES_FIELD(enable_low_latency_dxva, false)
    GPU_PREFERENCES_FIELD(enable_zero_copy_dxgi_video, true)
    GPU_PREFERENCES_FIELD(enable_nv12_dxgi_video, true)
    GPU_PREFERENCES_FIELD(disable_software_rasterizer, true)
    GPU_PREFERENCES_FIELD(log_gpu_control_list_decisions, true)
    GPU_PREFERENCES_FIELD(compile_shader_always_succeeds, true)
    GPU_PREFERENCES_FIELD(disable_gl_error_limit, true)
    GPU_PREFERENCES_FIELD(disable_glsl_translator, true)
    GPU_PREFERENCES_FIELD(disable_shader_name_hashing, true)
    GPU_PREFERENCES_FIELD(enable_gpu_command_logging, true)
    GPU_PREFERENCES_FIELD(enable_gpu_debugging, true)
    GPU_PREFERENCES_FIELD(enable_gpu_service_logging_gpu, true)
    GPU_PREFERENCES_FIELD(enable_gpu_driver_debug_logging, true)
    GPU_PREFERENCES_FIELD(disable_gpu_program_cache, true)
    GPU_PREFERENCES_FIELD(enforce_gl_minimums, true)
    GPU_PREFERENCES_FIELD(force_gpu_mem_available_bytes, 4096)
    GPU_PREFERENCES_FIELD(force_gpu_mem_discardable_limit_bytes, 8092)
    GPU_PREFERENCES_FIELD(gpu_program_cache_size,
                          kDefaultMaxProgramCacheMemoryBytes - 1)
    GPU_PREFERENCES_FIELD(disable_gpu_shader_disk_cache, true)
    GPU_PREFERENCES_FIELD(enable_threaded_texture_mailboxes, true)
    GPU_PREFERENCES_FIELD(gl_shader_interm_output, true)
    GPU_PREFERENCES_FIELD(enable_gpu_service_logging, true)
    GPU_PREFERENCES_FIELD(enable_gpu_service_tracing, true)
    GPU_PREFERENCES_FIELD(use_passthrough_cmd_decoder, true)
    GPU_PREFERENCES_FIELD(ignore_gpu_blocklist, true)
    GPU_PREFERENCES_FIELD(watchdog_starts_backgrounded, true)
    GPU_PREFERENCES_FIELD_ENUM(gr_context_type, GrContextType::kVulkan,
                               mojom::GrContextType::kVulkan)
    GPU_PREFERENCES_FIELD_ENUM(use_vulkan, VulkanImplementationName::kNative,
                               mojom::VulkanImplementationName::kNative)
    GPU_PREFERENCES_FIELD(vulkan_heap_memory_limit, 1);
    GPU_PREFERENCES_FIELD(vulkan_sync_cpu_memory_limit, 1);
    GPU_PREFERENCES_FIELD(enable_gpu_benchmarking_extension, true)
    GPU_PREFERENCES_FIELD(enable_webgpu, true)
    GPU_PREFERENCES_FIELD_ENUM(enable_dawn_backend_validation,
                               DawnBackendValidationLevel::kPartial,
                               mojom::DawnBackendValidationLevel::kPartial)
    GPU_PREFERENCES_FIELD(enable_perf_data_collection, true)
#if BUILDFLAG(IS_OZONE)
    GPU_PREFERENCES_FIELD_ENUM(message_pump_type, base::MessagePumpType::UI,
                               base::MessagePumpType::UI)
#endif
    GPU_PREFERENCES_FIELD(enable_native_gpu_memory_buffers, true);
    GPU_PREFERENCES_FIELD(force_separate_egl_display_for_webgl_testing, true);

    // Make sure every field is encoded/decoded.
    std::string encoded = input_prefs.ToSwitchValue();
    bool flag = decoded_prefs.FromSwitchValue(encoded);
    EXPECT_TRUE(flag);
    CheckGpuPreferencesEqual(input_prefs, decoded_prefs);
  }
}

// Helper test for decoding GPU preferences from a crash dump string.
TEST(GpuPreferencesTest, DISABLED_DecodePreferences) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kGpuPreferences)) {
    LOG(ERROR) << "Please specify the preferences to decode via "
               << switches::kGpuPreferences;
    return;
  }

  const auto preferences =
      command_line->GetSwitchValueASCII(switches::kGpuPreferences);

  gpu::GpuPreferences gpu_preferences;
  if (!gpu_preferences.FromSwitchValue(preferences)) {
    LOG(ERROR) << "Failed to decode preferences: " << preferences;
    return;
  }

  printf("GpuPreferences = {\n");
#define PRINT_BOOL(key) \
  printf("  %s: %s\n", #key, gpu_preferences.key ? "true" : "false")
#define PRINT_INT(key) \
  printf("  %s: %d\n", #key, static_cast<uint32_t>(gpu_preferences.key))

  PRINT_BOOL(disable_accelerated_video_decode);
  PRINT_BOOL(disable_accelerated_video_encode);
  PRINT_BOOL(gpu_startup_dialog);
  PRINT_BOOL(disable_gpu_watchdog);
  PRINT_BOOL(gpu_sandbox_start_early);
  PRINT_BOOL(enable_low_latency_dxva);
  PRINT_BOOL(enable_zero_copy_dxgi_video);
  PRINT_BOOL(enable_nv12_dxgi_video);
  PRINT_BOOL(disable_software_rasterizer);
  PRINT_BOOL(log_gpu_control_list_decisions);
  PRINT_BOOL(compile_shader_always_succeeds);
  PRINT_BOOL(disable_gl_error_limit);
  PRINT_BOOL(disable_glsl_translator);
  PRINT_BOOL(disable_shader_name_hashing);
  PRINT_BOOL(enable_gpu_command_logging);
  PRINT_BOOL(enable_gpu_debugging);
  PRINT_BOOL(enable_gpu_service_logging_gpu);
  PRINT_BOOL(enable_gpu_driver_debug_logging);
  PRINT_BOOL(disable_gpu_program_cache);
  PRINT_BOOL(enforce_gl_minimums);
  PRINT_INT(force_gpu_mem_available_bytes);
  PRINT_INT(force_gpu_mem_discardable_limit_bytes);
  PRINT_INT(gpu_program_cache_size);
  PRINT_BOOL(disable_gpu_shader_disk_cache);
  PRINT_BOOL(enable_threaded_texture_mailboxes);
  PRINT_BOOL(gl_shader_interm_output);
  PRINT_BOOL(enable_gpu_service_logging);
  PRINT_BOOL(enable_gpu_service_tracing);
  PRINT_BOOL(use_passthrough_cmd_decoder);
  PRINT_BOOL(ignore_gpu_blocklist);
  PRINT_BOOL(watchdog_starts_backgrounded);
  PRINT_INT(gr_context_type);
  PRINT_INT(use_vulkan);
  PRINT_INT(vulkan_heap_memory_limit);
  PRINT_INT(vulkan_sync_cpu_memory_limit);
  PRINT_BOOL(enable_gpu_benchmarking_extension);
  PRINT_BOOL(enable_webgpu);
  PRINT_INT(enable_dawn_backend_validation);
  PRINT_BOOL(enable_perf_data_collection);
#if BUILDFLAG(IS_OZONE)
  PRINT_INT(message_pump_type);
#endif
  PRINT_BOOL(enable_native_gpu_memory_buffers);
  PRINT_BOOL(force_separate_egl_display_for_webgl_testing);
  printf("}\n");
}

}  // namespace gpu
