// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_crash_keys.h"

#include "build/build_config.h"

namespace gpu {
namespace crash_keys {

#if !BUILDFLAG(IS_ANDROID)
crash_reporter::CrashKeyString<16> gpu_vendor_id("gpu-venid");
crash_reporter::CrashKeyString<16> gpu_device_id("gpu-devid");
crash_reporter::CrashKeyString<16> gpu_count("gpu_count");
#endif  // !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_WIN)
crash_reporter::CrashKeyString<16> gpu_sub_sys_id("gpu-subid");
crash_reporter::CrashKeyString<16> gpu_revision("gpu-rev");
#endif  // BUILDFLAG(IS_WIN)
crash_reporter::CrashKeyString<64> gpu_driver_version("gpu-driver");
crash_reporter::CrashKeyString<16> gpu_pixel_shader_version("gpu-psver");
crash_reporter::CrashKeyString<16> gpu_vertex_shader_version("gpu-vsver");
crash_reporter::CrashKeyString<16> gpu_generation_intel("gpu-generation-intel");
#if BUILDFLAG(IS_MAC)
crash_reporter::CrashKeyString<64> gpu_gl_version("gpu-glver");
#elif BUILDFLAG(IS_POSIX)
crash_reporter::CrashKeyString<256> gpu_vendor("gpu-gl-vendor");
crash_reporter::CrashKeyString<128> gpu_renderer("gpu-gl-renderer");
#endif
crash_reporter::CrashKeyString<4> gpu_gl_context_is_virtual(
    "gpu-gl-context-is-virtual");
crash_reporter::CrashKeyString<20> available_physical_memory_in_mb(
    "available-physical-memory-in-mb");
crash_reporter::CrashKeyString<1024> current_shader_0("current-shader-0");
crash_reporter::CrashKeyString<1024> current_shader_1("current-shader-1");
crash_reporter::CrashKeyString<1024> gpu_gl_error_message(
    "gpu-gl-error-message");
crash_reporter::CrashKeyString<4> gpu_watchdog_kill_after_power_resume(
    "gpu-watchdog-kill-after-power-resume");
crash_reporter::CrashKeyString<4> gpu_watchdog_crashed_in_gpu_init(
    "gpu-watchdog-crashed-in-gpu-init");
crash_reporter::CrashKeyString<16> num_of_processors("num-of-processors");
crash_reporter::CrashKeyString<64> gpu_thread("gpu-thread");
crash_reporter::CrashKeyString<128> list_of_hung_threads(
    "list-of-hung-threads");

}  // namespace crash_keys
}  // namespace gpu
