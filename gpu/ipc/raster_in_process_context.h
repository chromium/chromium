// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_RASTER_IN_PROCESS_CONTEXT_H_
#define GPU_IPC_RASTER_IN_PROCESS_CONTEXT_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/client/raster_implementation.h"
#include "gpu/command_buffer/service/command_buffer_task_executor.h"
#include "gpu/ipc/in_process_command_buffer.h"

namespace gpu {
class CommandBufferHelper;
class ContextSupport;
class ServiceTransferCache;
class SharedImageInterface;
class TransferBuffer;
struct GpuFeatureInfo;
struct SharedMemoryLimits;

// Runs client and server side command buffer code in process. Only supports
// RasterInterface.
class RasterInProcessContext {
 public:
  RasterInProcessContext();

  RasterInProcessContext(const RasterInProcessContext&) = delete;
  RasterInProcessContext& operator=(const RasterInProcessContext&) = delete;

  ~RasterInProcessContext();

  ContextResult Initialize(CommandBufferTaskExecutor* task_executor,
                           const ContextCreationAttribs& attribs,
                           const SharedMemoryLimits& memory_limits,
                           gpu::raster::GrShaderCache* gr_shader_cache,
                           GpuProcessShmCount* use_shader_cache_shm_count);

  const Capabilities& GetCapabilities() const;
  const GpuFeatureInfo& GetGpuFeatureInfo() const;

  // Allows direct access to the RasterImplementation so a
  // RasterInProcessContext can be used without making it current.
  gpu::raster::RasterImplementation* GetImplementation();

  ContextSupport* GetContextSupport();

  SharedImageInterface* GetSharedImageInterface();

  // Test only functions.
  ServiceTransferCache* GetTransferCacheForTest() const;
  InProcessCommandBuffer* GetCommandBufferForTest() const;
  int GetRasterDecoderIdForTest() const;

  // Test only function. Returns false if using passthrough decoding, which is
  // currently unsupported.
  static bool SupportedInTest();

 private:
  std::unique_ptr<CommandBufferHelper> helper_;
  std::unique_ptr<TransferBuffer> transfer_buffer_;
  std::unique_ptr<raster::RasterImplementation> raster_implementation_;
  std::unique_ptr<InProcessCommandBuffer> command_buffer_;
};

}  // namespace gpu

#endif  // GPU_IPC_RASTER_IN_PROCESS_CONTEXT_H_
