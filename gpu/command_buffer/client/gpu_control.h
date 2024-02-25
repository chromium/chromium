// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GPU_CONTROL_H_
#define GPU_COMMAND_BUFFER_CLIENT_GPU_CONTROL_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/functional/callback.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/gpu_export.h"
#include "ui/gfx/overlay_transform.h"

extern "C" typedef struct _ClientBuffer* ClientBuffer;
extern "C" typedef struct _ClientGpuFence* ClientGpuFence;

namespace base {
class Lock;
}

namespace gfx {
class GpuFence;
}

namespace gpu {
class GpuControlClient;
struct SyncToken;

// Common interface for GpuControl implementations.
class GPU_EXPORT GpuControl {
 public:
  GpuControl() = default;

  GpuControl(const GpuControl&) = delete;
  GpuControl& operator=(const GpuControl&) = delete;

  virtual ~GpuControl() = default;

  virtual void SetGpuControlClient(GpuControlClient* gpu_control_client) = 0;

  virtual const Capabilities& GetCapabilities() const = 0;

  virtual const GLCapabilities& GetGLCapabilities() const = 0;

  // Runs |callback| when a query created via glCreateQueryEXT() has cleared
  // passed the glEndQueryEXT() point.
  virtual void SignalQuery(uint32_t query, base::OnceClosure callback) = 0;
  // Cancels all quieries that havent been run via signalQuery.
  virtual void CancelAllQueries() = 0;

  virtual void CreateGpuFence(uint32_t gpu_fence_id, ClientGpuFence source) = 0;
  virtual void GetGpuFence(
      uint32_t gpu_fence_id,
      base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback) = 0;

  // Sets a lock this will be held on every callback from the GPU
  // implementation. This lock must be set and must be held on every call into
  // the GPU implementation if it is to be used from multiple threads. This
  // may not be supported with all implementations.
  virtual void SetLock(base::Lock*) = 0;

  // When this function returns it ensures all previously flushed work is
  // visible by the service. This command does this by sending a synchronous
  // IPC. Note just because the work is visible to the server does not mean
  // that it has been processed. This is only relevant for out of process
  // services and will be treated as a NOP for in process command buffers.
  virtual void EnsureWorkVisible() = 0;

  // The namespace and command buffer ID forms a unique pair for all existing
  // GpuControl (on client) and matches for the corresponding command buffer
  // (on server) in a single server process. The extra command buffer data can
  // be used for extra identification purposes. One usage is to store some
  // extra field to identify unverified sync tokens for the implementation of
  // the CanWaitUnverifiedSyncToken() function.
  virtual CommandBufferNamespace GetNamespaceID() const = 0;
  virtual CommandBufferId GetCommandBufferID() const = 0;

  // Flush any outstanding ordering barriers on all contexts.
  virtual void FlushPendingWork() = 0;

  // Generates a fence sync which should be inserted into the GL command stream.
  // When the service executes the fence sync it is released. Fence syncs are
  // shared with other contexts as sync tokens which encapsulate the fence sync
  // and the command buffer on which it was generated. Fence syncs need to be
  // flushed before they can be used by other contexts. Furthermore, the flush
  // must be verified before sending a sync token across channel boundaries.
  virtual uint64_t GenerateFenceSyncRelease() = 0;

  // Returns true if the service has released (executed) the fence sync. Some
  // implementations may support calling this from any thread without holding
  // the lock provided by the client.
  virtual bool IsFenceSyncReleased(uint64_t release) = 0;

  // Runs |callback| when sync token is signaled.
  virtual void SignalSyncToken(const SyncToken& sync_token,
                               base::OnceClosure callback) = 0;

  // This allows the command buffer proxy to mark the next flush with sync token
  // dependencies for the gpu scheduler, or to block prior to the flush in case
  // of android webview.
  virtual void WaitSyncToken(const SyncToken& sync_token) = 0;

  // Under some circumstances a sync token may be used which has not been
  // verified to have been flushed. For example, fence syncs queued on the same
  // channel as the wait command guarantee that the fence sync will be enqueued
  // first so does not need to be flushed.
  virtual bool CanWaitUnverifiedSyncToken(const SyncToken& sync_token) = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_GPU_CONTROL_H_
