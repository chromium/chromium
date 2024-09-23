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

  bool IsGpuWorkaroundEnabled(int32_t workaround) override;

  void CreateSurface(gl::SurfaceTexture*) override;

  void CreateAndBindContextProvider(base::OnceClosure callback) override;

  // All other public methods below must be called on the GL thread
  // (except when marked otherwise).

  void ResizeSurface(int width, int height) override;

  bool CopyMailboxToSurfaceAndSwap(const gpu::MailboxHolder& mailbox,
                                   const gfx::Transform& uv_transform) override;

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
  void InitializeRenderer();
  void DestroyContext();
  void DrawQuad(unsigned int textureHandle, const gfx::Transform& uv_transform);

  scoped_refptr<viz::ContextProvider> context_provider_;
  raw_ptr<gpu::gles2::GLES2Interface> gl_ = nullptr;
  raw_ptr<gpu::ContextSupport> context_support_ = nullptr;
  int surface_handle_ = gpu::kNullSurfaceHandle;
  // TODO(crbug.com/41385307): shouldn't have both of these closures
  // in the same class like this.
  base::OnceClosure on_context_bound_;

  // Only initialized if we have Surface (i.e surface_handle_ is not null).
  int surface_width_ = 0;
  int surface_height_ = 0;

  // If true, surface width/height is the intended size that should be applied
  // to the surface once it's ready for use.
  bool needs_resize_ = false;

  // A swap ID which is passed to GL swap. Incremented each call.
  uint64_t swap_id_ = 0;

  // Uniform handle for the UV transform used by DrawQuad.
  uint32_t uniform_uv_transform_handle_ = 0;

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
