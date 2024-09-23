// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_switching.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include <OpenGL/OpenGL.h>
#endif

#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_info.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_preference.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif  // BUILDFLAG(IS_MAC)

namespace gpu {

namespace {

#if BUILDFLAG(IS_MAC)
typedef CGLPixelFormatObj PlatformPixelFormatObj;
#else
typedef void* PlatformPixelFormatObj;
#endif  // BUILDFLAG(IS_MAC)

PlatformPixelFormatObj g_discrete_pixel_format_obj = nullptr;

void ForceDiscreteGPU() {
  if (g_discrete_pixel_format_obj)
    return;
#if BUILDFLAG(IS_MAC)
  CGLPixelFormatAttribute attribs[1];
  attribs[0] = static_cast<CGLPixelFormatAttribute>(0);
  GLint num_pixel_formats = 0;
  CGLChoosePixelFormat(attribs, &g_discrete_pixel_format_obj,
                       &num_pixel_formats);
#endif  // BUILDFLAG(IS_MAC)
}

}  // namespace anonymous

bool SwitchableGPUsSupported(const GPUInfo& gpu_info,
                             const base::CommandLine& command_line) {
#if BUILDFLAG(IS_MAC)
  if (command_line.HasSwitch(switches::kUseGL) &&
      (command_line.GetSwitchValueASCII(switches::kUseGL) !=
           gl::kGLImplementationANGLEName)) {
    return false;
  }
  // Always allow offline renderers on ARM-based macs.
  // https://crbug.com/1131312
  switch (base::mac::GetCPUType()) {
    case base::mac::CPUType::kArm:
    case base::mac::CPUType::kTranslatedIntel:
      return true;
    default:
      break;
  }
  if (gpu_info.secondary_gpus.size() != 1) {
    return false;
  }

  // Only advertise that we have two GPUs to the rest of Chrome's code if we
  // find an Intel GPU and some other vendor's GPU. Otherwise we don't
  // understand the configuration and don't deal well with it (an example being
  // the dual AMD GPUs in recent Mac Pros). Motivation is explained in:
  // http://crbug.com/380026#c70.
  const uint32_t kVendorIntel = 0x8086;
  return ((gpu_info.gpu.vendor_id == kVendorIntel &&
           gpu_info.secondary_gpus[0].vendor_id != kVendorIntel) ||
          (gpu_info.gpu.vendor_id != kVendorIntel &&
           gpu_info.secondary_gpus[0].vendor_id == kVendorIntel));
#else
  return false;
#endif  // BUILDFLAG(IS_MAC)
}

void InitializeSwitchableGPUs(
    const std::vector<int32_t>& driver_bug_workarounds) {
  gl::GLContext::SetSwitchableGPUsSupported();
  if (base::Contains(driver_bug_workarounds, FORCE_HIGH_PERFORMANCE_GPU)) {
    gl::GLSurface::SetForcedGpuPreference(gl::GpuPreference::kHighPerformance);
    ForceDiscreteGPU();
  } else if (base::Contains(driver_bug_workarounds, FORCE_LOW_POWER_GPU)) {
    gl::GLSurface::SetForcedGpuPreference(gl::GpuPreference::kLowPower);
  }
}

void StopForceDiscreteGPU() {
#if BUILDFLAG(IS_MAC)
  if (g_discrete_pixel_format_obj) {
    CGLReleasePixelFormat(g_discrete_pixel_format_obj);
    g_discrete_pixel_format_obj = nullptr;
  }
#endif  // BUILDFLAG(IS_MAC)
}

}  // namespace gpu
