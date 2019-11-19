// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_WEBGPU_DECODER_IMPL_H_
#define GPU_COMMAND_BUFFER_SERVICE_WEBGPU_DECODER_IMPL_H_

#include "gpu/gpu_gles2_export.h"

namespace gpu {

class CommandBufferServiceBase;
class DecoderClient;
class MemoryTracker;
class SharedImageManager;

namespace gles2 {
class Outputter;
}  // namespace gles2

namespace webgpu {

class WebGPUDecoder;

GPU_GLES2_EXPORT WebGPUDecoder* CreateWebGPUDecoderImpl(
    DecoderClient* client,
    CommandBufferServiceBase* command_buffer_service,
    SharedImageManager* shared_image_manager,
    MemoryTracker* memory_tracker,
    gles2::Outputter* outputter);

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_WEBGPU_DECODER_IMPL_H_
