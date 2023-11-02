// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_MEMORY_STATS_H_
#define GPU_IPC_COMMON_MEMORY_STATS_H_

// Provides access to the GPU information for the system
// on which chrome is currently running.

#include <stddef.h>

#include <map>

#include "base/process/process.h"
#include "gpu/gpu_export.h"

namespace gpu {

// Note: we use uint64_t instead of size_t for byte count because this struct
// is sent over IPC which could span 32 & 64 bit processes.
struct GPU_EXPORT VideoMemoryUsageStats {
  VideoMemoryUsageStats();
  VideoMemoryUsageStats(const VideoMemoryUsageStats& other);
  ~VideoMemoryUsageStats();

  struct GPU_EXPORT ProcessStats {
    ProcessStats();
    ~ProcessStats();

    // The bytes of GPU resources accessible by this process
    uint64_t video_memory;

    // Set to true if this process' GPU resource count is inflated because
    // it is counting other processes' resources (e.g, the GPU process has
    // duplicate set to true because it is the aggregate of all processes)
    bool has_duplicates;
  };
  typedef std::map<base::ProcessId, ProcessStats> ProcessMap;

  // A map of processes to their GPU resource consumption
  ProcessMap process_map;

  // The total amount of GPU memory allocated at the time of the request.
  uint64_t bytes_allocated;
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_MEMORY_STATS_H_
