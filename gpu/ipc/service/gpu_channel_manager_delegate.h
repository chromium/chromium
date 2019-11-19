// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_CHANNEL_MANAGER_DELEGATE_H_
#define GPU_IPC_SERVICE_GPU_CHANNEL_MANAGER_DELEGATE_H_

#include "gpu/command_buffer/common/constants.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/service/display_context.h"

class GURL;

namespace gpu {

// TODO(kylechar): Rename this class. It's used to provide GpuServiceImpl
// functionality to multiple classes in src/gpu/ so delegate is inaccurate.
class GpuChannelManagerDelegate {
 public:
  // Registers/unregistered display compositor contexts that don't have a GPU
  // channel and aren't tracked by GpuChannelManager.
  virtual void RegisterDisplayContext(DisplayContext* display_context) = 0;
  virtual void UnregisterDisplayContext(DisplayContext* display_context) = 0;

  // Force the loss of all GL contexts.
  virtual void LoseAllContexts() = 0;

  // Called on any successful context creation.
  virtual void DidCreateContextSuccessfully() = 0;

  // Tells the delegate that an offscreen context was created for the provided
  // |active_url|.
  virtual void DidCreateOffscreenContext(const GURL& active_url) = 0;

  // Notification from GPU that the channel is destroyed.
  virtual void DidDestroyChannel(int client_id) = 0;

  // Tells the delegate that an offscreen context was destroyed for the provided
  // |active_url|.
  virtual void DidDestroyOffscreenContext(const GURL& active_url) = 0;

  // Tells the delegate that a context was lost.
  virtual void DidLoseContext(bool offscreen,
                              error::ContextLostReason reason,
                              const GURL& active_url) = 0;

  // Tells the delegate to cache the given shader information in persistent
  // storage. The embedder is expected to repopulate the in-memory cache through
  // the respective GpuChannelManager API.
  virtual void StoreShaderToDisk(int32_t client_id,
                                 const std::string& key,
                                 const std::string& shader) = 0;

  // Cleanly exits the GPU process in response to an error. This will not exit
  // with in-process GPU as that would also exit the browser. This can only be
  // called from the GPU thread.
  virtual void MaybeExitOnContextLost() = 0;

  // Returns true if the GPU process is exiting. This can be called from any
  // thread.
  virtual bool IsExiting() const = 0;

  // Returns GPU Scheduler
  virtual gpu::Scheduler* GetGpuScheduler() = 0;

#if defined(OS_WIN)
  // Tells the delegate that |child_window| was created in the GPU process and
  // to send an IPC to make SetParent() syscall. This syscall is blocked by the
  // GPU sandbox and must be made in the browser process.
  virtual void SendCreatedChildWindow(SurfaceHandle parent_window,
                                      SurfaceHandle child_window) = 0;
#endif

 protected:
  virtual ~GpuChannelManagerDelegate() = default;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_CHANNEL_MANAGER_DELEGATE_H_
