// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_UTILS_H_
#define GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_UTILS_H_

#include <cstddef>

#include "gpu/gpu_gles2_export.h"

class SkImage;
struct SkImageInfo;
class SkSurface;

namespace skgpu::graphite {
class Context;
class Recorder;
}  // namespace skgpu::graphite

namespace gpu {

GPU_GLES2_EXPORT
void GraphiteFlush(skgpu::graphite::Context* context,
                   skgpu::graphite::Recorder* recorder);

GPU_GLES2_EXPORT
void GraphiteFlushAndSubmit(skgpu::graphite::Context* context,
                            skgpu::graphite::Recorder* recorder);

// Synchronously read pixels from a graphite image.
// Note this is for single plane image.
// TODO(crbug.com/40924444): Add a function to read multiplanar image.
GPU_GLES2_EXPORT
bool GraphiteReadPixelsSync(skgpu::graphite::Context* context,
                            skgpu::graphite::Recorder* recorder,
                            SkImage* image,
                            const SkImageInfo& dst_info,
                            void* dst_pointer,
                            size_t dst_bytes_per_row,
                            int src_x,
                            int src_y);

GPU_GLES2_EXPORT
bool GraphiteReadPixelsSync(skgpu::graphite::Context* context,
                            skgpu::graphite::Recorder* recorder,
                            SkSurface* surface,
                            const SkImageInfo& dst_info,
                            void* dst_pointer,
                            size_t dst_bytes_per_row,
                            int src_x,
                            int src_y);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_UTILS_H_
