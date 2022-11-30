// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_MEMORY_STATS_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_MEMORY_STATS_MOJOM_TRAITS_H_

#include "build/build_config.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/memory_stats.h"
#include "gpu/ipc/common/memory_stats.mojom-shared.h"

namespace mojo {

template <>
struct GPU_EXPORT StructTraits<gpu::mojom::VideoMemoryProcessStatsDataView,
                               gpu::VideoMemoryUsageStats::ProcessStats> {
  static uint64_t video_memory_bytes(
      const gpu::VideoMemoryUsageStats::ProcessStats& state) {
    return state.video_memory;
  }

  static bool has_duplicates(
      const gpu::VideoMemoryUsageStats::ProcessStats& state) {
    return state.has_duplicates;
  }

  static bool Read(gpu::mojom::VideoMemoryProcessStatsDataView data,
                   gpu::VideoMemoryUsageStats::ProcessStats* out) {
    out->video_memory = data.video_memory_bytes();
    out->has_duplicates = data.has_duplicates();
    return true;
  }
};

template <>
struct GPU_EXPORT StructTraits<gpu::mojom::VideoMemoryUsageStatsDataView,
                               gpu::VideoMemoryUsageStats> {
  static std::map<int32_t, gpu::VideoMemoryUsageStats::ProcessStats>
  process_map(const gpu::VideoMemoryUsageStats& stats) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA)
    std::map<int32_t, gpu::VideoMemoryUsageStats::ProcessStats> map;
    for (const auto& pair : stats.process_map)
      map[static_cast<int32_t>(pair.first)] = pair.second;
    return map;
#else
    return stats.process_map;
#endif
  }

  static uint64_t bytes_allocated(const gpu::VideoMemoryUsageStats& stats) {
    return stats.bytes_allocated;
  }

  static bool Read(gpu::mojom::VideoMemoryUsageStatsDataView data,
                   gpu::VideoMemoryUsageStats* out) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA)
    std::map<int32_t, gpu::VideoMemoryUsageStats::ProcessStats> process_map;
    if (!data.ReadProcessMap(&process_map))
      return false;
    for (const auto& pair : process_map)
      out->process_map[static_cast<base::ProcessId>(pair.first)] = pair.second;
#else
    if (!data.ReadProcessMap(&out->process_map))
      return false;
#endif
    out->bytes_allocated = data.bytes_allocated();
    return true;
  }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_MEMORY_STATS_MOJOM_TRAITS_H_
