// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_switching.h"

#if defined(OS_MACOSX)
#include <OpenGL/OpenGL.h>
#endif

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_info.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_preference.h"

namespace gpu {

namespace {

#if defined(OS_MACOSX)
typedef CGLPixelFormatObj PlatformPixelFormatObj;
#else
typedef void* PlatformPixelFormatObj;
#endif  // OS_MACOSX

PlatformPixelFormatObj g_discrete_pixel_format_obj = nullptr;

bool ContainsWorkaround(const std::vector<int32_t>& workarounds,
                        int32_t workaround) {
  return (std::find(workarounds.begin(), workarounds.end(), workaround) !=
          workarounds.end());
}

void ForceDiscreteGPU() {
  if (g_discrete_pixel_format_obj)
    return;
#if defined(OS_MACOSX)
  CGLPixelFormatAttribute attribs[1];
  attribs[0] = static_cast<CGLPixelFormatAttribute>(0);
  GLint num_pixel_formats = 0;
  CGLChoosePixelFormat(attribs, &g_discrete_pixel_format_obj,
                       &num_pixel_formats);
#endif  // OS_MACOSX
}

}  // namespace anonymous

bool SwitchableGPUsSupported(const GPUInfo& gpu_info,
                             const base::CommandLine& command_line) {
#if defined(OS_MACOSX)
  if (command_line.HasSwitch(switches::kUseGL) &&
      command_line.GetSwitchValueASCII(switches::kUseGL) !=
          gl::kGLImplementationDesktopName) {
    return false;
  }
  if (gpu_info.secondary_gpus.size() != 1) {
    return false;
  }
  // Only advertise that we have two GPUs to the rest of
  // Chrome's code if we find an Intel GPU and some other
  // vendor's GPU. Otherwise we don't understand the
  // configuration and don't deal well with it (an example being
  // the dual AMD GPUs in recent Mac Pros).
  const uint32_t kVendorIntel = 0x8086;
  return ((gpu_info.gpu.vendor_id == kVendorIntel &&
           gpu_info.secondary_gpus[0].vendor_id != kVendorIntel) ||
          (gpu_info.gpu.vendor_id != kVendorIntel &&
           gpu_info.secondary_gpus[0].vendor_id == kVendorIntel));
#else
  return false;
#endif  // OS_MACOSX
}

void InitializeSwitchableGPUs(
    const std::vector<int32_t>& driver_bug_workarounds) {
  gl::GLContext::SetSwitchableGPUsSupported();
  if (ContainsWorkaround(driver_bug_workarounds, FORCE_HIGH_PERFORMANCE_GPU)) {
    gl::GLContext::SetForcedGpuPreference(gl::GpuPreference::kHighPerformance);
    ForceDiscreteGPU();
  } else if (ContainsWorkaround(driver_bug_workarounds, FORCE_LOW_POWER_GPU)) {
    gl::GLContext::SetForcedGpuPreference(gl::GpuPreference::kLowPower);
  }
}

void StopForceDiscreteGPU() {
#if defined(OS_MACOSX)
  if (g_discrete_pixel_format_obj) {
    CGLReleasePixelFormat(g_discrete_pixel_format_obj);
    g_discrete_pixel_format_obj = nullptr;
  }
#endif  // OS_MACOSX
}

}  // namespace gpu
