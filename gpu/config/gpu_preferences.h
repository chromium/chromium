// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_PREFERENCES_H_
#define GPU_CONFIG_GPU_PREFERENCES_H_

#include <stddef.h>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "gpu/gpu_export.h"
#include "media/media_buildflags.h"
#include "ui/gfx/buffer_types.h"

#if BUILDFLAG(IS_OZONE)
#include "base/message_loop/message_pump_type.h"
#endif

namespace gpu {

// The size to set for the program cache for default and low-end device cases.
#if !BUILDFLAG(IS_ANDROID)
const size_t kDefaultMaxProgramCacheMemoryBytes = 6 * 1024 * 1024;
#else
const size_t kDefaultMaxProgramCacheMemoryBytes = 2 * 1024 * 1024;
const size_t kLowEndMaxProgramCacheMemoryBytes = 128 * 1024;
#endif

GPU_EXPORT size_t GetDefaultGpuDiskCacheSize();

enum class VulkanImplementationName : uint32_t {
  kNone = 0,
  kNative = 1,
  kForcedNative = 2,  // Cannot be overridden by GPU blocklist.
  kSwiftshader = 3,
  kLast = kSwiftshader,
};

enum class WebGPUAdapterName : uint32_t {
  kDefault = 0,
  kD3D11 = 1,
  kOpenGLES = 2,
  kSwiftShader = 3,
};

// Affecting how chromium handles GPUPowerPreference in
// GPURequestAdapterOptions.
enum class WebGPUPowerPreference : uint32_t {
  // No explicit power preference.
  kNone = 0,
  // Choose the preferred adapter when GPUPowerPreference is not given.
  // Has no impact when GPUPowerPreference is given.
  kDefaultLowPower = 1,
  kDefaultHighPerformance = 2,
  // Choose the forced adapter regardless of whether GPUPowerPreference is set
  // or not.
  kForceLowPower = 3,
  kForceHighPerformance = 4,
};

enum class GrContextType : uint32_t {
  kNone,
  kGL,      // Ganesh
  kVulkan,  // Ganesh
  kGraphiteDawn,
  kGraphiteMetal,
};

GPU_EXPORT std::string GrContextTypeToString(GrContextType type);

enum class DawnBackendValidationLevel : uint32_t {
  kDisabled = 0,
  kPartial = 1,
  kFull = 2,
};

// NOTE: if you modify this structure then you must also modify the
// following two files to keep them in sync:
//   src/gpu/ipc/common/gpu_preferences.mojom
//   src/gpu/ipc/common/gpu_preferences_mojom_traits.h
struct GPU_EXPORT GpuPreferences {
 public:
  GpuPreferences();

  GpuPreferences(const GpuPreferences& other);

  ~GpuPreferences();

  // Encode struct into a string so it can be passed as a commandline switch.
  std::string ToSwitchValue();

  // Decode the encoded string back to GpuPrefences struct.
  // If return false, |this| won't be touched.
  bool FromSwitchValue(const std::string& data);

  // ===================================
  // Settings from //content/public/common/content_switches.h

  // Disables hardware acceleration of video decode, where available.
  bool disable_accelerated_video_decode = false;

  // Disables hardware acceleration of video encode, where available.
  bool disable_accelerated_video_encode = false;

  // Causes the GPU process to display a dialog on launch.
  bool gpu_startup_dialog = false;

  // Disable the thread that crashes the GPU process if it stops responding to
  // messages.
  bool disable_gpu_watchdog = false;

  // Starts the GPU sandbox before creating a GL context.
  bool gpu_sandbox_start_early = false;

  // Enables using CODECAPI_AVLowLatencyMode. Windows only.
  bool enable_low_latency_dxva = true;

  // Enables support for avoiding copying DXGI NV12 textures. Windows only.
  bool enable_zero_copy_dxgi_video = false;

  // Enables support for outputting NV12 video frames. Windows only.
  bool enable_nv12_dxgi_video = false;

  // Disables the use of a 3D software rasterizer, for example, SwiftShader.
  bool disable_software_rasterizer = false;

  bool log_gpu_control_list_decisions = false;

  // ===================================
  // Settings from //gpu/command_buffer/service/gpu_switches.cc

  // Always return success when compiling a shader. Linking will still fail.
  bool compile_shader_always_succeeds = false;

  // Disable the GL error log limit.
  bool disable_gl_error_limit = false;

  // Disable the GLSL translator.
  bool disable_glsl_translator = false;

  // Turn off user-defined name hashing in shaders.
  bool disable_shader_name_hashing = false;

  // Turn on Logging GPU commands.
  bool enable_gpu_command_logging = false;

  // Turn on Calling GL Error after every command.
  bool enable_gpu_debugging = false;

  // Enable GPU service logging.
  bool enable_gpu_service_logging_gpu = false;

  // Enable logging of GPU driver debug messages.
  bool enable_gpu_driver_debug_logging = false;

  // Turn off gpu program caching
  bool disable_gpu_program_cache = false;

  // Enforce GL minimums.
  bool enforce_gl_minimums = false;

  // Sets the total amount of memory that may be allocated for GPU resources.
  uint32_t force_gpu_mem_available_bytes = 0u;

  // Sets the maximum discardable cache size limit for GPU resources.
  uint32_t force_gpu_mem_discardable_limit_bytes = 0u;

  // Sets maximum texture size.
  uint32_t force_max_texture_size = 0u;

  // Sets the maximum size of the in-memory gpu program cache, in kb
  uint32_t gpu_program_cache_size = kDefaultMaxProgramCacheMemoryBytes;

  // Disables the GPU shader on disk cache.
  bool disable_gpu_shader_disk_cache = false;

  // Simulates shared textures when share groups are not available.
  // Not available everywhere.
  bool enable_threaded_texture_mailboxes = false;

  // Include ANGLE's intermediate representation (AST) output in shader
  // compilation info logs.
  bool gl_shader_interm_output = false;

  // ===================================
  // Settings from //gpu/config/gpu_switches.h

  // Enables the use of SurfaceControl for overlays on Android.
  bool enable_android_surface_control = false;

  // ===================================
  // Settings from //ui/gl/gl_switches.h

  // Turns on GPU logging (debug build only).
  bool enable_gpu_service_logging = false;

  // Turns on calling TRACE for every GL call.
  bool enable_gpu_service_tracing = false;

  // Use the Pass-through command decoder, skipping all validation and state
  // tracking.
  bool use_passthrough_cmd_decoder = false;

  // ===================================
  // Settings from //gpu/config/gpu_switches.h

  // Ignores GPU blocklist.
  bool ignore_gpu_blocklist = false;

  // Start the watchdog suspended, as the app is already backgrounded and won't
  // send a background/suspend signal.
  bool watchdog_starts_backgrounded = false;

  // ===================================
  // Settings from //gpu/command_buffer/service/gpu_switches.h
  // The type of the GrContext or Graphite Context.
  GrContextType gr_context_type = GrContextType::kGL;

  // Use Vulkan for rasterization and display compositing.
  VulkanImplementationName use_vulkan = VulkanImplementationName::kNone;

  // Enable using vulkan protected memory.
  bool enable_vulkan_protected_memory = false;

  // Use vulkan VK_KHR_surface for presenting.
  bool disable_vulkan_surface = false;

  // If Vulkan initialization has failed, do not fallback to GL. This is for
  // testing in order to detect regressions which crash Vulkan.
  bool disable_vulkan_fallback_to_gl_for_testing = false;

  // Heap memory limit for Vulkan. Allocations will fail when this limit is
  // reached for a heap.
  uint32_t vulkan_heap_memory_limit = 0u;

  // Sync CPU memory limit for Vulkan. Submission of GPU work will be
  // synchronize with the CPU in order to free released memory immediately
  // when this limit is reached.
  uint32_t vulkan_sync_cpu_memory_limit = 0u;

  // ===================================
  // Settings from //cc/base/switches.h
  // Enable the GPU benchmarking extension; used by tests only.
  bool enable_gpu_benchmarking_extension = false;

  // Enable the WebGPU command buffer.
  bool enable_webgpu = false;

  // Enable usage of unsafe WebGPU features.
  bool enable_unsafe_webgpu = false;

  // Enable usage of WebGPU features intended only for use during development.
  bool enable_webgpu_developer_features = false;

  // Enable usage of experimental WebGPU features that would eventually land in
  // the WebGPU spec.
  bool enable_webgpu_experimental_features = false;

  // Enable validation layers in Dawn backends.
  DawnBackendValidationLevel enable_dawn_backend_validation =
      DawnBackendValidationLevel::kDisabled;

  // The adapter to use for WebGPU content.
  WebGPUAdapterName use_webgpu_adapter = WebGPUAdapterName::kDefault;

  // The adapter selecting strategy related to GPUPowerPreference.
  WebGPUPowerPreference use_webgpu_power_preference =
      WebGPUPowerPreference::kNone;

  // Force the use of WebGPU Compatibility mode for all WebGPU content.
  bool force_webgpu_compat = false;

  // The Dawn features(toggles) enabled on the creation of Dawn devices.
  std::vector<std::string> enabled_dawn_features_list;

  // The Dawn features(toggles) disabled on the creation of Dawn devices.
  std::vector<std::string> disabled_dawn_features_list;

  // Enable measuring blocked time on GPU Main thread
  bool enable_gpu_blocked_time_metric = false;

  // Enable collecting perf data for device categorization purpose. Currently
  // only enabled on Windows platform for the info collection GPU process.
  bool enable_perf_data_collection = false;

#if BUILDFLAG(IS_OZONE)
  // Determines message pump type for the GPU thread.
  base::MessagePumpType message_pump_type = base::MessagePumpType::DEFAULT;
#endif

  // ===================================
  // Settings from //ui/gfx/switches.h

  // Enable native CPU-mappable GPU memory buffer support on Linux.
  bool enable_native_gpu_memory_buffers = false;

  // Disables oppr debug crash dumps.
  bool disable_oopr_debug_crash_dump = false;

  // Forces the use of a separate EGL display for WebGL contexts even when one
  // GPU is used.
  bool force_separate_egl_display_for_webgl_testing = false;

  // Please update gpu_preferences_unittest.cc when making additions or
  // changes to this struct.
};

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_PREFERENCES_H_
