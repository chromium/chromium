// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_info_collector.h"

#import <Metal/Metal.h>

#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "third_party/angle/src/gpu_info_util/SystemInfo.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_utils.h"

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

void RecordReadWriteMetalTexturesSupportedHistogram() {
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
}  // namespace

bool CollectContextGraphicsInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  TRACE_EVENT0("gpu", "gpu_info_collector::CollectGraphicsInfo");

  RecordReadWriteMetalTexturesSupportedHistogram();

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
