// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_SKIA_UTILS_H_
#define GPU_COMMAND_BUFFER_COMMON_SKIA_UTILS_H_

#include <cstdint>
#include <optional>

#include "gpu/raster_export.h"

class GrDirectContext;

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}
}  // namespace base

namespace gpu {
namespace raster {

// Dumps memory usage from the |context| to |pmd|. A |tracing_guid| can be used
// if these resources are referenced across processes for sharing across dumps.
RASTER_EXPORT void DumpGrMemoryStatistics(
    const GrDirectContext* context,
    base::trace_event::ProcessMemoryDump* pmd,
    std::optional<uint64_t> tracing_guid);

// Dumps a single"skia/grpu_resources/context_0x{&context}" entry with total
// cache usage. Designed for background dumps.
RASTER_EXPORT void DumpBackgroundGrMemoryStatistics(
    const GrDirectContext* context,
    base::trace_event::ProcessMemoryDump* pmd);

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_SKIA_UTILS_H_
