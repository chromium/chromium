// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_WEBGPU_DECODER_IMPL_H_
#define GPU_COMMAND_BUFFER_SERVICE_WEBGPU_DECODER_IMPL_H_

#include "gpu/gpu_gles2_export.h"

#include "base/memory/scoped_refptr.h"

namespace gpu {

class CommandBufferServiceBase;
class DecoderClient;
struct GpuPreferences;
class IsolationKeyProvider;
class MemoryTracker;
class SharedContextState;
class SharedImageManager;

namespace gles2 {
class Outputter;
}  // namespace gles2

namespace webgpu {

struct DawnCacheOptions;
class WebGPUDecoder;

GPU_GLES2_EXPORT WebGPUDecoder* CreateWebGPUDecoderImpl(
    DecoderClient* client,
    CommandBufferServiceBase* command_buffer_service,
    SharedImageManager* shared_image_manager,
    MemoryTracker* memory_tracker,
    gles2::Outputter* outputter,
    const GpuPreferences& gpu_preferences,
    scoped_refptr<SharedContextState> shared_context_state,
    const DawnCacheOptions& dawn_cache_options,
    IsolationKeyProvider* isolation_key_provider);

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_WEBGPU_DECODER_IMPL_H_
