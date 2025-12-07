// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_WEBGPU_DECODER_H_
#define GPU_COMMAND_BUFFER_SERVICE_WEBGPU_DECODER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/service/common_decoder.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/ipc/common/gpu_disk_cache_type.h"

namespace gpu {

class DecoderClient;
struct GpuFeatureInfo;
struct GpuPreferences;
class IsolationKeyProvider;
class MemoryTracker;
class SharedContextState;
class SharedImageManager;

namespace gles2 {
class Outputter;
}  // namespace gles2

namespace webgpu {

class DawnCachingInterfaceFactory;

// Options specifically passed for Dawn caching;
struct DawnCacheOptions {
  raw_ptr<DawnCachingInterfaceFactory> caching_interface_factory = nullptr;
  std::optional<GpuDiskCacheHandle> handle = {};
};

class GPU_GLES2_EXPORT WebGPUDecoder : public DecoderContext,
                                       public CommonDecoder {
 public:
  static std::unique_ptr<WebGPUDecoder> Create(
      DecoderClient* client,
      CommandBufferServiceBase* command_buffer_service,
      SharedImageManager* shared_image_manager,
      scoped_refptr<MemoryTracker> memory_tracker,
      gles2::Outputter* outputter,
      const GpuPreferences& gpu_preferences,
      scoped_refptr<SharedContextState> shared_context_state,
      const DawnCacheOptions& dawn_cache_options = {},
      IsolationKeyProvider* isolation_key_provider = nullptr);

  WebGPUDecoder(const WebGPUDecoder&) = delete;
  WebGPUDecoder& operator=(const WebGPUDecoder&) = delete;

  ~WebGPUDecoder() override;

  virtual ContextResult Initialize(const GpuFeatureInfo& gpu_feature_info) = 0;

 protected:
  WebGPUDecoder(DecoderClient* client,
                CommandBufferServiceBase* command_buffer_service,
                gles2::Outputter* outputter);
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_WEBGPU_DECODER_H_
