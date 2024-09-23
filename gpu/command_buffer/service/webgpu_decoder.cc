// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/webgpu_decoder.h"

#include "gpu/command_buffer/service/shared_context_state.h"
#include "ui/gl/buildflags.h"

#if BUILDFLAG(USE_DAWN)
#include "gpu/command_buffer/service/webgpu_decoder_impl.h"
#endif

namespace gpu {
namespace webgpu {

// static
WebGPUDecoder* WebGPUDecoder::Create(
    DecoderClient* client,
    CommandBufferServiceBase* command_buffer_service,
    SharedImageManager* shared_image_manager,
    MemoryTracker* memory_tracker,
    gles2::Outputter* outputter,
    const GpuPreferences& gpu_preferences,
    scoped_refptr<SharedContextState> shared_context_state,
    const DawnCacheOptions& dawn_cache_options,
    IsolationKeyProvider* isolation_key_provider) {
#if BUILDFLAG(USE_DAWN)
  return CreateWebGPUDecoderImpl(
      client, command_buffer_service, shared_image_manager, memory_tracker,
      outputter, gpu_preferences, std::move(shared_context_state),
      dawn_cache_options, isolation_key_provider);
#else
  NOTREACHED_IN_MIGRATION();
  return nullptr;
#endif
}

WebGPUDecoder::WebGPUDecoder(DecoderClient* client,
                             CommandBufferServiceBase* command_buffer_service,
                             gles2::Outputter* outputter)
    : CommonDecoder(client, command_buffer_service) {}

WebGPUDecoder::~WebGPUDecoder() {}

ContextResult WebGPUDecoder::Initialize(
    const scoped_refptr<gl::GLSurface>& surface,
    const scoped_refptr<gl::GLContext>& context,
    bool offscreen,
    const gles2::DisallowedFeatures& disallowed_features,
    const ContextCreationAttribs& attrib_helper) {
  return ContextResult::kSuccess;
}

}  // namespace webgpu
}  // namespace gpu
