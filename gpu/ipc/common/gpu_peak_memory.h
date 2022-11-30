// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_PEAK_MEMORY_H_
#define GPU_IPC_COMMON_GPU_PEAK_MEMORY_H_

namespace gpu {

enum class GpuPeakMemoryAllocationSource {
  UNKNOWN,
  COMMAND_BUFFER,
  SHARED_CONTEXT_STATE,
  SHARED_IMAGE_STUB,
  SKIA,
  GPU_PEAK_MEMORY_ALLOCATION_SOURCE_MAX = SKIA,
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_PEAK_MEMORY_H_
