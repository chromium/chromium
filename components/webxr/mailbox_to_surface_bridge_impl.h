// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBXR_MAILBOX_TO_SURFACE_BRIDGE_IMPL_H_
#define COMPONENTS_WEBXR_MAILBOX_TO_SURFACE_BRIDGE_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "device/vr/android/mailbox_to_surface_bridge.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/android/scoped_java_surface.h"

namespace gpu {
class ContextSupport;

namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace viz {
class ContextProvider;
}

namespace webxr {

class MailboxToSurfaceBridgeImpl : public device::MailboxToSurfaceBridge {
 public:
  // It's OK to create an object instance and pass it to a different thread,
  // i.e. to enable dependency injection for a unit test, but all methods on it
  // must be called consistently on a single GL thread. This is verified by
  // DCHECKs.
  MailboxToSurfaceBridgeImpl();

  MailboxToSurfaceBridgeImpl(const MailboxToSurfaceBridgeImpl&) = delete;
  MailboxToSurfaceBridgeImpl& operator=(const MailboxToSurfaceBridgeImpl&) =
      delete;

  ~MailboxToSurfaceBridgeImpl() override;

  bool IsConnected() override;

  void CreateAndBindContextProvider(base::OnceClosure callback) override;

  // All other public methods below must be called on the GL thread
  // (except when marked otherwise).
  void GenSyncToken(gpu::SyncToken* out_sync_token) override;

  void WaitSyncToken(const gpu::SyncToken& sync_token) override;

  void WaitForClientGpuFence(gfx::GpuFence&) override;

  void CreateGpuFence(const gpu::SyncToken& sync_token,
                      base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)>
                          callback) override;

  scoped_refptr<gpu::ClientSharedImage> CreateSharedImage(
      gfx::GpuMemoryBufferHandle buffer_handle,
      gfx::BufferFormat buffer_format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      gpu::SharedImageUsageSet usage,
      gpu::SyncToken& sync_token) override;

  void DestroySharedImage(
      const gpu::SyncToken& sync_token,
      scoped_refptr<gpu::ClientSharedImage> shared_image) override;

 private:
  void BindContextProviderToCurrentThread();
  void OnContextAvailableOnUiThread(
      scoped_refptr<viz::ContextProvider> provider);

  scoped_refptr<viz::ContextProvider> context_provider_;
  raw_ptr<gpu::gles2::GLES2Interface> gl_ = nullptr;
  raw_ptr<gpu::ContextSupport> context_support_ = nullptr;

  // TODO(crbug.com/41385307): shouldn't have both of these closures
  // in the same class like this.
  base::OnceClosure on_context_bound_;

  // A task runner for the GL thread
  scoped_refptr<base::SingleThreadTaskRunner> gl_thread_task_runner_;

  // Must be last.
  base::WeakPtrFactory<MailboxToSurfaceBridgeImpl> weak_ptr_factory_{this};
};

class MailboxToSurfaceBridgeFactoryImpl
    : public device::MailboxToSurfaceBridgeFactory {
 public:
  std::unique_ptr<device::MailboxToSurfaceBridge> Create() const override;
};

}  // namespace webxr

#endif  // COMPONENTS_WEBXR_MAILBOX_TO_SURFACE_BRIDGE_IMPL_H_
