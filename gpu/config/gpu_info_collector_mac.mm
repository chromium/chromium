// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_info_collector.h"

#import <Metal/Metal.h>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "gpu/config/gpu_finch_features.h"
#include "third_party/angle/src/gpu_info_util/SystemInfo.h"
#include "ui/gfx/extension_set.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gl_version_info.h"

namespace gpu {

namespace {

// The enums is used for an UMA histogram so we should never reorder entries or
// remove unused values.
enum class MetalReadWriteTextureSupportTier {
  kUnknown = 0,
  kTier0_NoSupport = 1,
  kTier1_R32Formats = 2,
  kTier2_AdditionalFormats = 3,
  kMaxValue = kTier2_AdditionalFormats,
};

// The enums is used for an UMA histogram so we should never reorder entries or
// remove unused values.
enum class MetalFamilyMac {
  kMac1 = 0,
  kMac2 = 1,
  kMaxValue = kMac2,
};

MetalFamilyMac GetMetalFamilyMac(id<MTLDevice> device) {
  if ([device supportsFamily:MTLGPUFamilyMac2]) {
    return MetalFamilyMac::kMac2;
  }
  return MetalFamilyMac::kMac1;
}

void RecordHistograms() {
  // Metal tiers are `MTLReadWriteTextureTier[None|1|2]` which correspond to the
  // integers 0, 1, and 2. The enum `MetalReadWriteTextureSupportTier` was
  // written to use integers one higher than the macOS API constants so that it
  // could support the concept of "unknown". Nowadays, `kUnknown` will only be
  // logged in the case where `MTLCopyAllDevices()` returns an empty array,
  // perhaps when running in an environment like VMWare?
  NSUInteger best_tier = 0;

  NSArray<id<MTLDevice>>* devices = MTLCopyAllDevices();
  for (id<MTLDevice> device in devices) {
    best_tier = std::max(best_tier, device.readWriteTextureSupport + 1);

    UMA_HISTOGRAM_ENUMERATION("Gpu.Metal.FamilyMac", GetMetalFamilyMac(device));
  }

  UMA_HISTOGRAM_ENUMERATION(
      "Gpu.Metal.ReadWriteTextureSupport",
      static_cast<MetalReadWriteTextureSupportTier>(best_tier));
}

bool IsLowPowerGpu(const GPUInfo::GPUDevice& gpu) {
  // Apple GPUs are considered low power. This may not be the case in the
  // future.
  switch (gpu.vendor_id) {
    case 0x8086:  // Intel
    case 0x106b:  // Apple
      return true;
    default:
      return false;
  }
}

bool IsHighPerformanceGpu(const GPUInfo::GPUDevice& gpu) {
  return !gpu.IsSoftwareRenderer() && !IsLowPowerGpu(gpu);
}

// Marks the GPU whose IORegistry entry ID is |registry_id| as active, and all
// others as inactive. On macOS, GPUDevice::system_device_id is the IORegistry
// entry ID, which is also what MTLDevice.registryID returns.
bool IdentifyActiveGPUWithRegistryID(GPUInfo* gpu_info, uint64_t registry_id) {
  if (!registry_id) {
    return false;
  }
  GPUInfo::GPUDevice* active_gpu = nullptr;
  if (gpu_info->gpu.system_device_id == registry_id) {
    active_gpu = &gpu_info->gpu;
  }
  for (auto& secondary_gpu : gpu_info->secondary_gpus) {
    if (!active_gpu && secondary_gpu.system_device_id == registry_id) {
      active_gpu = &secondary_gpu;
    }
  }
  if (!active_gpu) {
    return false;
  }
  gpu_info->gpu.active = false;
  for (auto& secondary_gpu : gpu_info->secondary_gpus) {
    secondary_gpu.active = false;
  }
  active_gpu->active = true;
  return true;
}

// Collects context graphics info from ANGLE's Metal device and EGL display,
// without creating a GL context. Returns false, without guaranteeing which
// fields were set, if the Metal device or the GL strings aren't available.
bool CollectMetalGraphicsInfo(GPUInfo* gpu_info, gl::GLDisplay* display) {
  TRACE_EVENT0("gpu", "gpu_info_collector::CollectMetalGraphicsInfo");
  gl::GLDisplayEGL* egl_display =
      display ? display->GetAs<gl::GLDisplayEGL>() : nullptr;
  if (!egl_display) {
    return false;
  }
  // Returns nil unless ANGLE runs on Metal.
  id<MTLDevice> metal_device = egl_display->GetMetalDevice();
  if (!metal_device) {
    return false;
  }

  if (!CollectGraphicsInfoFromANGLEDisplay(gpu_info, display) ||
      gpu_info->gl_renderer.empty() || gpu_info->gl_version.empty()) {
    return false;
  }

  // ANGLE's GL_SHADING_LANGUAGE_VERSION is
  // "OpenGL ES GLSL ES 3.00 (ANGLE <version>)" for the ES 3.0 context created
  // in CollectGraphicsInfoGL().
  const std::string glsl_version = "3.00";
  gpu_info->pixel_shader_version = glsl_version;
  gpu_info->vertex_shader_version = glsl_version;

  // ANGLE's Metal device is the GPU that's actually in use.
  if (!IdentifyActiveGPUWithRegistryID(gpu_info, metal_device.registryID)) {
    MarkPrimaryGpuActive(gpu_info);
  }

  GPUInfo::GPUDevice& active_gpu = gpu_info->active_gpu();

  // Unlike on Windows, where the basic info collection sets each GPU's
  // device_string, nothing else sets the GPU strings on macOS. Set the active
  // GPU's from the GL strings. They're needed to identify GPUs without a PCI
  // device ID (e.g. Apple Silicon).
  active_gpu.vendor_string = gpu_info->gl_vendor;
  active_gpu.device_string = gpu_info->gl_renderer;

  // As in CollectGraphicsInfoGL(), derive the driver vendor and version from
  // ANGLE's GL strings if they're not known yet.
  gl::GLVersionInfo gl_info(gpu_info->gl_version.c_str(),
                            gpu_info->gl_renderer.c_str(), gfx::ExtensionSet());
  if (!gl_info.driver_vendor.empty() && active_gpu.driver_vendor.empty()) {
    active_gpu.driver_vendor = gl_info.driver_vendor;
  }
  if (!gl_info.driver_version.empty() && active_gpu.driver_version.empty()) {
    active_gpu.driver_version = gl_info.driver_version;
  }
  return true;
}
}  // namespace

bool CollectContextGraphicsInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  TRACE_EVENT0("gpu", "gpu_info_collector::CollectGraphicsInfo");

  RecordHistograms();

  if (gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal &&
      base::FeatureList::IsEnabled(features::kGpuInitOptimization) &&
      CollectMetalGraphicsInfo(gpu_info, gl::GetDefaultDisplayEGL())) {
    return true;
  }
  return CollectGraphicsInfoGL(gpu_info, gl::GetDefaultDisplayEGL());
}

bool CollectBasicGraphicsInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  angle::SystemInfo system_info;
  bool success = angle::GetSystemInfo(&system_info);
  FillGPUInfoFromSystemInfo(gpu_info, &system_info);

  if (gpu_info->GpuCount() > 1) {
    if (IsLowPowerGpu(gpu_info->gpu))
      gpu_info->gpu.gpu_preference = gl::GpuPreference::kLowPower;
    else if (IsHighPerformanceGpu(gpu_info->gpu))
      gpu_info->gpu.gpu_preference = gl::GpuPreference::kHighPerformance;
    for (auto& gpu : gpu_info->secondary_gpus) {
      if (IsLowPowerGpu(gpu))
        gpu.gpu_preference = gl::GpuPreference::kLowPower;
      else if (IsHighPerformanceGpu(gpu))
        gpu.gpu_preference = gl::GpuPreference::kHighPerformance;
    }
  }

  return success;
}

}  // namespace gpu
