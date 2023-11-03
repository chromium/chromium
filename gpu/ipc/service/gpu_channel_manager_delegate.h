// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_CHANNEL_MANAGER_DELEGATE_H_
#define GPU_IPC_SERVICE_GPU_CHANNEL_MANAGER_DELEGATE_H_

#include "build/build_config.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/common/gpu_disk_cache_type.h"
#include "gpu/ipc/common/surface_handle.h"
#include "third_party/blink/public/common/tokens/tokens.h"

class GURL;

namespace gpu {

// TODO(kylechar): Rename this class. It's used to provide GpuServiceImpl
// functionality to multiple classes in src/gpu/ so delegate is inaccurate.
class GpuChannelManagerDelegate {
 public:
  // Force the loss of all GL contexts.
  virtual void LoseAllContexts() = 0;

  // Called on any successful context creation.
  virtual void DidCreateContextSuccessfully() = 0;

  // Tells the delegate that an offscreen context was created for the provided
  // |active_url|.
  virtual void DidCreateOffscreenContext(const GURL& active_url) = 0;

  // Notification from GPU that the channel is destroyed.
  virtual void DidDestroyChannel(int client_id) = 0;

  // Notification that all GPU channels are shutdown properly.
  // Note this is NOT called in error conditions such as losing channel due to
  // context loss, or from debug messages.
  virtual void DidDestroyAllChannels() = 0;

  // Tells the delegate that an offscreen context was destroyed for the provided
  // |active_url|.
  virtual void DidDestroyOffscreenContext(const GURL& active_url) = 0;

  // Tells the delegate that a context was lost.
  virtual void DidLoseContext(error::ContextLostReason reason,
                              const GURL& active_url) = 0;

  // Tells the delegate to cache the given blob information in persistent
  // storage. The embedder is expected to repopulate the in-memory cache through
  // the respective GpuChannelManager API.
  virtual void StoreBlobToDisk(const gpu::GpuDiskCacheHandle& handle,
                               const std::string& key,
                               const std::string& shader) = 0;

  // Tells the delegate to ask for a unique isolation key given a client id and
  // identifying token on the client.
  using GetIsolationKeyCallback =
      base::OnceCallback<void(const std::string& isolation_key)>;
  virtual void GetIsolationKey(int client_id,
                               const blink::WebGPUExecutionContextToken& token,
                               GetIsolationKeyCallback cb) = 0;

  // Cleanly exits the GPU process in response to an error. `synthetic_loss` is
  // true iff the context loss event was generated internally by Chrome rather
  // than being a true backend context loss.  In most cases this will not exit
  // with in-process GPU as that would also exit the browser (in some cases the
  // delegate might cause a deliberate crash if it determines that the context
  // loss is irrecoverable and that crashing is better than entering a context
  // loss loop). This can only be called from the GPU thread.
  virtual void MaybeExitOnContextLost(
      bool synthetic_loss,
      error::ContextLostReason context_lost_reason) = 0;

  // Returns true if the GPU process is exiting. This can be called from any
  // thread.
  virtual bool IsExiting() const = 0;

  // Returns GPU Scheduler
  virtual gpu::Scheduler* GetGpuScheduler() = 0;

 protected:
  virtual ~GpuChannelManagerDelegate() = default;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_CHANNEL_MANAGER_DELEGATE_H_
