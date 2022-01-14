// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_info_collector.h"

#include "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "third_party/angle/src/gpu_info_util/SystemInfo.h"

#import <Metal/Metal.h>

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
  // Metal tiers go 0, 1, 2, but we reserve 0 for when macOS is less then 10.13
  // and we can't query.
  NSUInteger best_tier = 0;

  if (@available(macOS 10.13, *)) {
    base::scoped_nsobject<NSArray<id<MTLDevice>>> devices(MTLCopyAllDevices());
    for (id<MTLDevice> device in devices.get()) {
      best_tier = std::max(best_tier, [device readWriteTextureSupport] + 1);
    }
  }

  UMA_HISTOGRAM_ENUMERATION(
      "Gpu.Metal.ReadWriteTextureSupport",
      static_cast<MetalReadWriteTextureSupportTier>(best_tier));
}
}

bool CollectContextGraphicsInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  TRACE_EVENT0("gpu", "gpu_info_collector::CollectGraphicsInfo");

  gpu_info->macos_specific_texture_target =
      gpu::GetPlatformSpecificTextureTarget();

  RecordReadWriteMetalTexturesSupportedHistogram();

  return CollectGraphicsInfoGL(gpu_info);
}

bool CollectBasicGraphicsInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  angle::SystemInfo system_info;
  bool success = angle::GetSystemInfo(&system_info);
  FillGPUInfoFromSystemInfo(gpu_info, &system_info);
  return success;
}

}  // namespace gpu
