// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_WEBGPU_COMMAND_BUFFER_STUB_H_
#define GPU_IPC_SERVICE_WEBGPU_COMMAND_BUFFER_STUB_H_

#include "base/memory/weak_ptr.h"
#include "gpu/ipc/service/command_buffer_stub.h"

namespace gpu {

class GPU_IPC_SERVICE_EXPORT WebGPUCommandBufferStub final
    : public CommandBufferStub {
 public:
  WebGPUCommandBufferStub(GpuChannel* channel,
                          const mojom::CreateCommandBufferParams& init_params,
                          CommandBufferId command_buffer_id,
                          SequenceId sequence_id,
                          int32_t stream_id,
                          int32_t route_id);

  WebGPUCommandBufferStub(const WebGPUCommandBufferStub&) = delete;
  WebGPUCommandBufferStub& operator=(const WebGPUCommandBufferStub&) = delete;

  ~WebGPUCommandBufferStub() override;

  // This must leave the GL context associated with the newly-created
  // CommandBufferStub current, so the GpuChannel can initialize
  // the gpu::Capabilities.
  gpu::ContextResult Initialize(
      CommandBufferStub* share_group,
      const mojom::CreateCommandBufferParams& init_params,
      base::UnsafeSharedMemoryRegion shared_state_shm) override;
  MemoryTracker* GetContextGroupMemoryTracker() const override;
  base::WeakPtr<CommandBufferStub> AsWeakPtr() override;

 private:
  void OnSwapBuffers(uint64_t swap_id, uint32_t flags) override;

  base::WeakPtrFactory<WebGPUCommandBufferStub> weak_ptr_factory_{this};
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_WEBGPU_COMMAND_BUFFER_STUB_H_
