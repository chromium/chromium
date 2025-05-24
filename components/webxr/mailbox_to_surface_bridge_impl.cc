// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webxr/mailbox_to_surface_bridge_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/gpu/context_provider.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_surface_tracker.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/android/surface_texture.h"

namespace webxr {

MailboxToSurfaceBridgeImpl::MailboxToSurfaceBridgeImpl() {
  DVLOG(1) << __func__;
}

MailboxToSurfaceBridgeImpl::~MailboxToSurfaceBridgeImpl() {
  DVLOG(1) << __func__;
}

bool MailboxToSurfaceBridgeImpl::IsConnected() {
  return context_provider_ && gl_ && context_support_;
}

void MailboxToSurfaceBridgeImpl::OnContextAvailableOnUiThread(
    scoped_refptr<viz::ContextProvider> provider) {
  DVLOG(1) << __func__;
  // Must save a reference to the viz::ContextProvider to keep it alive,
  // otherwise the GL context created from it becomes invalid on its
  // destruction.
  context_provider_ = std::move(provider);

  DCHECK(on_context_bound_);
  gl_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MailboxToSurfaceBridgeImpl::BindContextProviderToCurrentThread,
          base::Unretained(this)));
}

void MailboxToSurfaceBridgeImpl::BindContextProviderToCurrentThread() {
  auto result = context_provider_->BindToCurrentSequence();
  if (result != gpu::ContextResult::kSuccess) {
    DLOG(ERROR) << "Failed to init viz::ContextProvider";
    return;
  }

  gl_ = context_provider_->ContextGL();
  context_support_ = context_provider_->ContextSupport();

  if (!gl_) {
    DLOG(ERROR) << "Did not get a GL context";
    return;
  }
  if (!context_support_) {
    DLOG(ERROR) << "Did not get a ContextSupport";
    return;
  }

  DVLOG(1) << __func__ << ": Context ready";
  if (on_context_bound_) {
    std::move(on_context_bound_).Run();
  }
}

void MailboxToSurfaceBridgeImpl::CreateAndBindContextProvider(
    base::OnceClosure on_bound_callback) {
  gl_thread_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  on_context_bound_ = std::move(on_bound_callback);

  // The callback to run in this thread. It is necessary to keep |surface| alive
  // until the context becomes available. So pass it on to the callback, so that
  // it stays alive, and is destroyed on the same thread once done.
  auto callback =
      base::BindOnce(&MailboxToSurfaceBridgeImpl::OnContextAvailableOnUiThread,
                     weak_ptr_factory_.GetWeakPtr());

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(
                     [](content::Compositor::ContextProviderCallback callback) {
                       content::Compositor::CreateContextProvider(
                           gpu::SharedMemoryLimits::ForMailboxContext(),
                           std::move(callback));
                     },
                     std::move(callback)));
}

void MailboxToSurfaceBridgeImpl::GenSyncToken(gpu::SyncToken* out_sync_token) {
  TRACE_EVENT0("gpu", "GenSyncToken");
  DCHECK(IsConnected());
  gl_->GenSyncTokenCHROMIUM(out_sync_token->GetData());
}

void MailboxToSurfaceBridgeImpl::WaitSyncToken(
    const gpu::SyncToken& sync_token) {
  TRACE_EVENT0("gpu", "WaitSyncToken");
  DCHECK(IsConnected());
  gl_->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
}

void MailboxToSurfaceBridgeImpl::WaitForClientGpuFence(
    gfx::GpuFence& gpu_fence) {
  TRACE_EVENT0("gpu", "WaitForClientGpuFence");
  DCHECK(IsConnected());
  GLuint id = gl_->CreateClientGpuFenceCHROMIUM(gpu_fence.AsClientGpuFence());
  gl_->WaitGpuFenceCHROMIUM(id);
  gl_->DestroyGpuFenceCHROMIUM(id);
}

void MailboxToSurfaceBridgeImpl::CreateGpuFence(
    const gpu::SyncToken& sync_token,
    base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback) {
  TRACE_EVENT0("gpu", "CreateGpuFence");
  DCHECK(IsConnected());
  gl_->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  GLuint id = gl_->CreateGpuFenceCHROMIUM();
  context_support_->GetGpuFence(id, std::move(callback));
  gl_->DestroyGpuFenceCHROMIUM(id);
}

scoped_refptr<gpu::ClientSharedImage>
MailboxToSurfaceBridgeImpl::CreateSharedImage(
    gfx::GpuMemoryBufferHandle buffer_handle,
    gfx::BufferFormat buffer_format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    gpu::SharedImageUsageSet usage,
    gpu::SyncToken& sync_token) {
  TRACE_EVENT0("gpu", "CreateSharedImage");
  DCHECK(IsConnected());

  auto* sii = context_provider_->SharedImageInterface();
  DCHECK(sii);

  CHECK_EQ(buffer_format, gfx::BufferFormat::RGBA_8888);
  auto client_shared_image = sii->CreateSharedImage(
      {viz::SinglePlaneFormat::kRGBA_8888, size, color_space, usage,
       "WebXrMailboxToSurfaceBridge"},
      std::move(buffer_handle));
  CHECK(client_shared_image);
  sync_token = sii->GenVerifiedSyncToken();
  DCHECK(client_shared_image->GetTextureTarget() == GL_TEXTURE_2D);
  return client_shared_image;
}

void MailboxToSurfaceBridgeImpl::DestroySharedImage(
    const gpu::SyncToken& sync_token,
    scoped_refptr<gpu::ClientSharedImage> shared_image) {
  TRACE_EVENT0("gpu", "CreateSharedImage");
  DCHECK(IsConnected());
  DCHECK(shared_image);

  auto* sii = context_provider_->SharedImageInterface();
  DCHECK(sii);
  sii->DestroySharedImage(sync_token, std::move(shared_image));
}

std::unique_ptr<device::MailboxToSurfaceBridge>
MailboxToSurfaceBridgeFactoryImpl::Create() const {
  return std::make_unique<MailboxToSurfaceBridgeImpl>();
}

}  // namespace webxr
