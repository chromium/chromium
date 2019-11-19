// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_WEBGPU_IN_PROCESS_CONTEXT_H_
#define GPU_IPC_WEBGPU_IN_PROCESS_CONTEXT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "gpu/ipc/command_buffer_task_executor.h"
#include "gpu/ipc/in_process_command_buffer.h"

namespace base {
class TestSimpleTaskRunner;
}  // namespace base

namespace gpu {
class CommandBufferHelper;
class ServiceTransferCache;
class TransferBuffer;
struct GpuFeatureInfo;
struct SharedMemoryLimits;

namespace webgpu {
class WebGPUInterface;
class WebGPUImplementation;
}  // namespace webgpu

// Runs client and server side command buffer code in process. Only supports
// WebGPUInterface.
class WebGPUInProcessContext {
 public:
  WebGPUInProcessContext();
  ~WebGPUInProcessContext();

  // |attrib_list| must be null or a NONE-terminated list of attribute/value
  // pairs. |gpu_channel_manager| should be non-null when used in the GPU
  // process.
  ContextResult Initialize(
      CommandBufferTaskExecutor* task_executor,
      const ContextCreationAttribs& attribs,
      const SharedMemoryLimits& memory_limits,
      GpuMemoryBufferManager* gpu_memory_buffer_manager,
      ImageFactory* image_factory,
      GpuChannelManagerDelegate* gpu_channel_manager_delegate);

  const Capabilities& GetCapabilities() const;
  const GpuFeatureInfo& GetGpuFeatureInfo() const;

  // Allows direct access to the WebGPUImplementation so a
  // WebGPUInProcessContext can be used without making it current.
  gpu::webgpu::WebGPUInterface* GetImplementation();
  base::TestSimpleTaskRunner* GetTaskRunner();

  // Test only functions.
  ServiceTransferCache* GetTransferCacheForTest() const;
  InProcessCommandBuffer* GetCommandBufferForTest() const;

 private:
  std::unique_ptr<CommandBufferHelper> helper_;
  std::unique_ptr<TransferBuffer> transfer_buffer_;
  std::unique_ptr<webgpu::WebGPUImplementation> webgpu_implementation_;
  std::unique_ptr<InProcessCommandBuffer> command_buffer_;
  scoped_refptr<base::TestSimpleTaskRunner> client_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(WebGPUInProcessContext);
};

}  // namespace gpu

#endif  // GPU_IPC_WEBGPU_IN_PROCESS_CONTEXT_H_
