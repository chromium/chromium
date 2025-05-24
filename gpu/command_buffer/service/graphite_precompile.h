// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_PRECOMPILE_H_
#define GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_PRECOMPILE_H_

#include <memory>

#include "gpu/gpu_gles2_export.h"

namespace skgpu::graphite {
class PrecompileContext;
}  // namespace skgpu::graphite

namespace gpu {

GPU_GLES2_EXPORT
void GraphitePerformPrecompilation(
    std::unique_ptr<skgpu::graphite::PrecompileContext> precompileContext);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_PRECOMPILE_H_
