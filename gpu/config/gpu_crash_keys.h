// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_CRASH_KEYS_H_
#define GPU_CONFIG_GPU_CRASH_KEYS_H_

#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "gpu/config/gpu_config_export.h"

namespace gpu {
namespace crash_keys {

// Keys that can be used for crash reporting.
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<16> gpu_vendor_id;
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<16> gpu_device_id;
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<16> npu_vendor_id;
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<16> npu_device_id;
#if !BUILDFLAG(IS_ANDROID)
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<16> gpu_count;
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<16> npu_count;
#endif  // !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_WIN)
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<16> gpu_sub_sys_id;
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<16> gpu_revision;
#endif  // BUILDFLAG(IS_WIN)
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<64> gpu_driver_version;
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<64> npu_driver_version;
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<16>
    gpu_pixel_shader_version;
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<16>
    gpu_vertex_shader_version;
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<16>
    gpu_generation_intel;
#if BUILDFLAG(IS_MAC)
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<64> gpu_gl_version;
#elif BUILDFLAG(IS_POSIX)
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<256> gpu_vendor;
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<128> gpu_renderer;
#endif
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<4>
    gpu_gl_context_is_virtual;
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<20>
    available_physical_memory_in_mb;
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<1024> current_shader_0;
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<1024> current_shader_1;
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<1024>
    gpu_gl_error_message;
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<4>
    gpu_watchdog_kill_after_power_resume;
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<4>
    gpu_watchdog_crashed_in_gpu_init;
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<16> num_of_processors;
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<64> gpu_thread;
extern GPU_CONFIG_EXPORT crash_reporter::CrashKeyString<128>
    list_of_hung_threads;
}  // namespace crash_keys
}  // namespace gpu

#endif  // GPU_CONFIG_GPU_CRASH_KEYS_H_
