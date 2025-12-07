// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/arc_shared_image_interface.h"

#include "base/notimplemented.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/experiences/arc/arc_features.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/gl_context.h"

namespace gpu {

namespace {

bool MakeContextCurrentOnGpuThread(SharedContextState* context_state,
                                   bool needs_gl /*=false*/) {
  if (!context_state) {
    return false;
  }

  if (context_state->context_lost()) {
    return false;
  }

  // |shared_image_factory_| never writes to the surface, so pass nullptr to
  // improve performance. https://crbug.com/457431
  auto* context = context_state->real_context();
  if (context->IsCurrent(nullptr)) {
    return !context_state->CheckResetStatus(needs_gl);
  }
  return context_state->MakeCurrent(/*surface=*/nullptr, needs_gl);
}

}  // namespace

// static
scoped_refptr<ArcSharedImageInterface> ArcSharedImageInterface::Create(
    GpuChannelManager* gpu_channel_manager,
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner) {
  CHECK(gpu_task_runner->BelongsToCurrentThread());

  gpu::ContextResult result;
  scoped_refptr<SharedContextState> context_state =
      gpu_channel_manager->GetSharedContextState(&result);

  // Some of the backings require a current GL context to be created.
  if (!::gpu::MakeContextCurrentOnGpuThread(context_state.get(),
                                            /*needs_gl=*/true)) {
    return nullptr;
  }

  return base::MakeRefCounted<ArcSharedImageInterface>(
      std::make_unique<gpu::SharedImageFactory>(
          gpu_channel_manager->gpu_preferences(),
          gpu_channel_manager->gpu_driver_bug_workarounds(),
          gpu_channel_manager->gpu_feature_info(), context_state.get(),
          gpu_channel_manager->shared_image_manager(),
          context_state->memory_tracker(),
          /*is_for_display_compositor=*/false),
      std::move(gpu_task_runner));
}

ArcSharedImageInterface::ArcSharedImageInterface(
    std::unique_ptr<SharedImageFactory> shared_image_factory,
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner)
    : shared_image_factory_(std::move(shared_image_factory)),
      gpu_task_runner_(std::move(gpu_task_runner)) {}

ArcSharedImageInterface::~ArcSharedImageInterface() {
  CHECK(gpu_task_runner_->BelongsToCurrentThread());
  if (shared_image_factory_->HasImages()) {
    // Some of the backings might require a current GL context to be destroyed.
    bool have_context = MakeContextCurrentOnGpuThread(/*needs_gl=*/true);
    shared_image_factory_->DestroyAllSharedImages(have_context);
  }
  shared_image_factory_.reset();
}

scoped_refptr<ClientSharedImage> ArcSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    gpu::SurfaceHandle surface_handle,
    gfx::BufferUsage buffer_usage,
    gfx::GpuMemoryBufferHandle buffer_handle) {
  if (encountered_error_.load(std::memory_order_relaxed)) {
    return nullptr;
  }

  auto client_buffer_handle = buffer_handle.Clone();
  auto mailbox = Mailbox::Generate();
  // Copy which can be modified.
  SharedImageInfo si_info_copy = si_info;
  // Set CPU read/write usage based on buffer usage.
  si_info_copy.meta.usage |= GetCpuSIUsage(buffer_usage);

  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ArcSharedImageInterface::CreateSharedImageOnGpuThread,
                     base::Unretained(this), mailbox, si_info_copy,
                     buffer_usage, std::move(buffer_handle)));

  return base::MakeRefCounted<ClientSharedImage>(
      mailbox, si_info_copy, gpu::SyncToken(),
      GpuMemoryBufferHandleInfo(std::move(client_buffer_handle), buffer_usage),
      holder_);
}

void ArcSharedImageInterface::CreateSharedImageOnGpuThread(
    const Mailbox& mailbox,
    const SharedImageInfo& si_info,
    gfx::BufferUsage buffer_usage,
    gfx::GpuMemoryBufferHandle buffer_handle) {
  if (!MakeContextCurrentOnGpuThread()) {
    encountered_error_.store(true, std::memory_order_relaxed);
    return;
  }

  if (!shared_image_factory_->CreateSharedImage(
          mailbox, si_info.meta.format, si_info.meta.size,
          si_info.meta.color_space, si_info.meta.surface_origin,
          si_info.meta.alpha_type, si_info.meta.usage,
          std::move(si_info.debug_label), std::move(buffer_handle))) {
    shared_image_factory_->shared_context_state()->MarkContextLost();
    encountered_error_.store(true, std::memory_order_relaxed);
    return;
  }
}

void ArcSharedImageInterface::DestroySharedImage(
    const SyncToken& sync_token,
    scoped_refptr<ClientSharedImage> client_shared_image) {
  NOTREACHED();
}

scoped_refptr<ClientSharedImage> ArcSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    gpu::SurfaceHandle surface_handle,
    std::optional<SharedImagePoolId> pool_id /*=std::nullopt*/) {
  NOTREACHED();
}
scoped_refptr<ClientSharedImage> ArcSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    base::span<const uint8_t> pixel_data) {
  NOTREACHED();
}
scoped_refptr<ClientSharedImage> ArcSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    SurfaceHandle surface_handle,
    gfx::BufferUsage buffer_usage,
    std::optional<SharedImagePoolId> pool_id /*=std::nullopt*/) {
  NOTREACHED();
}
scoped_refptr<ClientSharedImage> ArcSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    gfx::GpuMemoryBufferHandle buffer_handle) {
  NOTREACHED();
}
scoped_refptr<ClientSharedImage>
ArcSharedImageInterface::CreateSharedImageForMLTensor(
    std::string debug_label,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    gpu::SharedImageUsageSet usage) {
  NOTREACHED();
}
scoped_refptr<ClientSharedImage>
ArcSharedImageInterface::CreateSharedImageForSoftwareCompositor(
    const SharedImageInfo& si_info) {
  NOTREACHED();
}
void ArcSharedImageInterface::UpdateSharedImage(const SyncToken& sync_token,
                                                const Mailbox& mailbox) {
  NOTREACHED();
}
void ArcSharedImageInterface::UpdateSharedImage(
    const SyncToken& sync_token,
    std::unique_ptr<gfx::GpuFence> acquire_fence,
    const Mailbox& mailbox) {
  NOTREACHED();
}
void ArcSharedImageInterface::DestroySharedImage(const SyncToken& sync_token,
                                                 const Mailbox& mailbox) {
  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ArcSharedImageInterface::DestroySharedImageOnGpuThread,
                     base::Unretained(this), mailbox));
}

void ArcSharedImageInterface::DestroySharedImageOnGpuThread(
    const Mailbox& mailbox) {
  if (!MakeContextCurrentOnGpuThread()) {
    return;
  }

  if (!shared_image_factory_->DestroySharedImage(mailbox)) {
    shared_image_factory_->shared_context_state()->MarkContextLost();
  }
}
scoped_refptr<ClientSharedImage> ArcSharedImageInterface::ImportSharedImage(
    ExportedSharedImage exported_shared_image) {
  NOTREACHED();
}
SyncToken ArcSharedImageInterface::GenUnverifiedSyncToken() {
  NOTREACHED();
}
SyncToken ArcSharedImageInterface::GenVerifiedSyncToken() {
  NOTREACHED();
}
void ArcSharedImageInterface::VerifySyncToken(SyncToken& sync_token) {
  NOTREACHED();
}
void ArcSharedImageInterface::WaitSyncToken(const SyncToken& sync_token) {
  NOTREACHED();
}
bool ArcSharedImageInterface::CanVerifySyncToken(const SyncToken& sync_token) {
  NOTREACHED();
}
void ArcSharedImageInterface::VerifyFlush() {
  NOTREACHED();
}

const SharedImageCapabilities& ArcSharedImageInterface::GetCapabilities() {
  NOTREACHED();
}

bool ArcSharedImageInterface::MakeContextCurrentOnGpuThread(
    bool needs_gl /*=false*/) {
  return ::gpu::MakeContextCurrentOnGpuThread(
      shared_image_factory_->shared_context_state(), needs_gl);
}

}  // namespace gpu
