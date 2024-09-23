// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_PEAK_MEMORY_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_GPU_PEAK_MEMORY_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_peak_memory.h"
#include "gpu/ipc/common/gpu_peak_memory.mojom-shared.h"

namespace mojo {

template <>
struct GPU_EXPORT EnumTraits<gpu::mojom::GpuPeakMemoryAllocationSource,
                             gpu::GpuPeakMemoryAllocationSource> {
  static gpu::mojom::GpuPeakMemoryAllocationSource ToMojom(
      gpu::GpuPeakMemoryAllocationSource gpu_peak_memory_allocation_source) {
    switch (gpu_peak_memory_allocation_source) {
      case gpu::GpuPeakMemoryAllocationSource::UNKNOWN:
        return gpu::mojom::GpuPeakMemoryAllocationSource::UNKNOWN;
      case gpu::GpuPeakMemoryAllocationSource::COMMAND_BUFFER:
        return gpu::mojom::GpuPeakMemoryAllocationSource::COMMAND_BUFFER;
      case gpu::GpuPeakMemoryAllocationSource::SHARED_CONTEXT_STATE:
        return gpu::mojom::GpuPeakMemoryAllocationSource::SHARED_CONTEXT_STATE;
      case gpu::GpuPeakMemoryAllocationSource::SHARED_IMAGE_STUB:
        return gpu::mojom::GpuPeakMemoryAllocationSource::SHARED_IMAGE_STUB;
      case gpu::GpuPeakMemoryAllocationSource::SKIA:
        return gpu::mojom::GpuPeakMemoryAllocationSource::SKIA;
    }
    NOTREACHED_IN_MIGRATION()
        << "Invalid GpuPeakMemoryAllocationSource:"
        << static_cast<int>(gpu_peak_memory_allocation_source);
    return gpu::mojom::GpuPeakMemoryAllocationSource::UNKNOWN;
  }

  static bool FromMojom(gpu::mojom::GpuPeakMemoryAllocationSource input,
                        gpu::GpuPeakMemoryAllocationSource* out) {
    switch (input) {
      case gpu::mojom::GpuPeakMemoryAllocationSource::UNKNOWN:
        *out = gpu::GpuPeakMemoryAllocationSource::UNKNOWN;
        return true;
      case gpu::mojom::GpuPeakMemoryAllocationSource::COMMAND_BUFFER:
        *out = gpu::GpuPeakMemoryAllocationSource::COMMAND_BUFFER;
        return true;
      case gpu::mojom::GpuPeakMemoryAllocationSource::SHARED_CONTEXT_STATE:
        *out = gpu::GpuPeakMemoryAllocationSource::SHARED_CONTEXT_STATE;
        return true;
      case gpu::mojom::GpuPeakMemoryAllocationSource::SHARED_IMAGE_STUB:
        *out = gpu::GpuPeakMemoryAllocationSource::SHARED_IMAGE_STUB;
        return true;
      case gpu::mojom::GpuPeakMemoryAllocationSource::SKIA:
        *out = gpu::GpuPeakMemoryAllocationSource::SKIA;
        return true;
    }
    NOTREACHED_IN_MIGRATION()
        << "Invalid GpuPeakMemoryAllocationSource: " << input;
    return false;
  }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_GPU_PEAK_MEMORY_MOJOM_TRAITS_H_
