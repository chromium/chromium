// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPAPI_COMMAND_BUFFER_PROXY_H_
#define PPAPI_PROXY_PPAPI_COMMAND_BUFFER_PROXY_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/functional/callback.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/host_resource.h"

namespace IPC {
class Message;
}

namespace ppapi {
namespace proxy {

class SerializedHandle;

class PPAPI_PROXY_EXPORT PpapiCommandBufferProxy : public gpu::CommandBuffer,
                                                   public gpu::GpuControl {
 public:
  PpapiCommandBufferProxy(const HostResource& resource,
                          InstanceData::FlushInfo* flush_info,
                          LockedSender* sender,
                          const gpu::Capabilities& capabilities,
                          const gpu::GLCapabilities& gl_capabilities,
                          SerializedHandle shared_state,
                          gpu::CommandBufferId command_buffer_id);

  PpapiCommandBufferProxy(const PpapiCommandBufferProxy&) = delete;
  PpapiCommandBufferProxy& operator=(const PpapiCommandBufferProxy&) = delete;

  ~PpapiCommandBufferProxy() override;

  // gpu::CommandBuffer implementation:
  State GetLastState() override;
  void Flush(int32_t put_offset) override;
  void OrderingBarrier(int32_t put_offset) override;
  State WaitForTokenInRange(int32_t start, int32_t end) override;
  State WaitForGetOffsetInRange(uint32_t set_get_buffer_count,
                                int32_t start,
                                int32_t end) override;
  void SetGetBuffer(int32_t transfer_buffer_id) override;
  scoped_refptr<gpu::Buffer> CreateTransferBuffer(
      uint32_t size,
      int32_t* id,
      uint32_t alignment = 0,
      gpu::TransferBufferAllocationOption option =
          gpu::TransferBufferAllocationOption::kLoseContextOnOOM) override;
  void DestroyTransferBuffer(int32_t id) override;
  void ForceLostContext(gpu::error::ContextLostReason reason) override;

  // gpu::GpuControl implementation:
  void SetGpuControlClient(gpu::GpuControlClient*) override;
  const gpu::Capabilities& GetCapabilities() const override;
  const gpu::GLCapabilities& GetGLCapabilities() const override;
  void SignalQuery(uint32_t query, base::OnceClosure callback) override;
  void CancelAllQueries() override;
  void CreateGpuFence(uint32_t gpu_fence_id, ClientGpuFence source) override;
  void GetGpuFence(uint32_t gpu_fence_id,
                   base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)>
                       callback) override;
  void SetLock(base::Lock*) override;
  void EnsureWorkVisible() override;
  gpu::CommandBufferNamespace GetNamespaceID() const override;
  gpu::CommandBufferId GetCommandBufferID() const override;
  void FlushPendingWork() override;
  uint64_t GenerateFenceSyncRelease() override;
  bool IsFenceSyncReleased(uint64_t release) override;
  void SignalSyncToken(const gpu::SyncToken& sync_token,
                       base::OnceClosure callback) override;
  void WaitSyncToken(const gpu::SyncToken& sync_token) override;
  bool CanWaitUnverifiedSyncToken(const gpu::SyncToken& sync_token) override;

 private:
  bool Send(IPC::Message* msg);
  void UpdateState(const gpu::CommandBuffer::State& state, bool success);

  // Try to read an updated copy of the state from shared memory.
  void TryUpdateState();

  // The shared memory area used to update state.
  gpu::CommandBufferSharedState* shared_state() const;

  void FlushInternal();

  const gpu::CommandBufferId command_buffer_id_;

  gpu::Capabilities capabilities_;
  gpu::GLCapabilities gl_capabilities_;
  State last_state_;
  base::WritableSharedMemoryMapping shared_state_mapping_;

  HostResource resource_;
  InstanceData::FlushInfo* flush_info_;
  LockedSender* sender_;

  uint64_t next_fence_sync_release_;
  uint64_t pending_fence_sync_release_;
  uint64_t flushed_fence_sync_release_;
  uint64_t validated_fence_sync_release_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPAPI_COMMAND_BUFFER_PROXY_H_
