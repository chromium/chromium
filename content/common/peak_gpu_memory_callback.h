// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_PEAK_GPU_MEMORY_CALLBACK_H_
#define CONTENT_COMMON_PEAK_GPU_MEMORY_CALLBACK_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "components/input/peak_gpu_memory_tracker.h"
#include "gpu/ipc/common/gpu_peak_memory.h"

namespace content {

// Callback provided to the GpuService, which will be notified of the
// |peak_memory| used since GpuService started tracking GPU memory.
//
// This callback reports the peak memory usage in UMA Histograms, categorizing
// it by the provided |usage| type.
//
// Parameters:
// - |usage|: Indicates the category of GPU memory usage being tracked.
// - |testing_callback|: (Optional) Closure used by some tests to synchronize
//                       with the work done here on the UI thread.
// - |peak_memory|: The total peak GPU memory usage in bytes.
// - |allocation_per_source|: A breakdown of the peak memory usage, showing how
//                            much was allocated by each source.
void PeakGpuMemoryCallback(
    input::PeakGpuMemoryTracker::Usage usage,
    base::OnceClosure testing_callback,
    const uint64_t peak_memory,
    const base::flat_map<gpu::GpuPeakMemoryAllocationSource, uint64_t>&
        allocation_per_source);
}  // namespace content

#endif  // CONTENT_COMMON_PEAK_GPU_MEMORY_CALLBACK_H_
