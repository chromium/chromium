// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_channel_shared_image_interface.h"

#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_interface_in_process_base.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"

#if BUILDFLAG(IS_ANDROID)
#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "gpu/command_buffer/service/shared_image/android_video_image_backing.h"
#include "gpu/command_buffer/service/stream_texture_shared_image_interface.h"
#endif

namespace gpu {

namespace {

// Generates unique IDs for GpuChannelSharedImageInterface.
base::AtomicSequenceNumber g_next_id;

}  // namespace

GpuChannelSharedImageInterface::GpuChannelSharedImageInterface(
    base::WeakPtr<SharedImageStub> shared_image_stub)
    : SharedImageInterfaceInProcessBase(
          CommandBufferNamespace::GPU_CHANNEL_SHARED_IMAGE_INTERFACE,
          CommandBufferIdFromChannelAndRoute(
              shared_image_stub->channel()->client_id(),
              g_next_id.GetNext() + 1),
          /*verify_creation_sync_token=*/true,
          shared_image_stub->factory()->MakeCapabilities()),
      shared_image_stub_(shared_image_stub),
      scheduler_(shared_image_stub->channel()->scheduler()),
      sequence_(scheduler_->CreateSequence(
          SchedulingPriority::kLow,
          shared_image_stub->channel()->task_runner(),
          CommandBufferNamespace::GPU_CHANNEL_SHARED_IMAGE_INTERFACE,
          command_buffer_id())) {}

GpuChannelSharedImageInterface::~GpuChannelSharedImageInterface() {
  scheduler_->DestroySequence(sequence_);
}

// Public functions specific to GpuChannelSharedImageInterface:
#if BUILDFLAG(IS_ANDROID)
scoped_refptr<ClientSharedImage>
GpuChannelSharedImageInterface::CreateSharedImageForAndroidVideo(
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    scoped_refptr<StreamTextureSharedImageInterface> image,
    scoped_refptr<RefCountedLock> drdc_lock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  if (!shared_image_stub_) {
    return nullptr;
  }

  auto mailbox = Mailbox::Generate();

  scoped_refptr<SharedContextState> shared_context =
      shared_image_stub_->shared_context_state();

  if (shared_context->context_lost()) {
    return nullptr;
  }

  auto shared_image_backing = AndroidVideoImageBacking::Create(
      mailbox, size, color_space, kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
      /*debug_label=*/"SIForAndroidVideo", std::move(image),
      std::move(shared_context), std::move(drdc_lock));
  SharedImageMetadata metadata{shared_image_backing->format(),
                               shared_image_backing->size(),
                               shared_image_backing->color_space(),
                               shared_image_backing->surface_origin(),
                               shared_image_backing->alpha_type(),
                               shared_image_backing->usage()};

  // Register it with shared image mailbox. This keeps |shared_image_backing|
  // around until its destruction cb is called.
  DCHECK(shared_image_stub_->channel()
             ->gpu_channel_manager()
             ->shared_image_manager());
  shared_image_stub_->factory()->RegisterBacking(
      std::move(shared_image_backing));

  SharedImageInfo info(metadata, /*debug_label=*/"SIForAndroidVideo");
  return base::WrapRefCounted<ClientSharedImage>(new ClientSharedImage(
      mailbox, info, GenVerifiedSyncToken(), holder_, GL_TEXTURE_EXTERNAL_OES));
}
#endif

#if BUILDFLAG(IS_WIN)
scoped_refptr<ClientSharedImage>
GpuChannelSharedImageInterface::CreateSharedImageForD3D11Video(
    const SharedImageInfo& si_info,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,
    scoped_refptr<gpu::DXGISharedHandleState> dxgi_shared_handle_state,
    size_t array_slice,
    const bool is_thread_safe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  if (!shared_image_stub_) {
    return nullptr;
  }

  auto mailbox = Mailbox::Generate();
  auto metadata = si_info.meta;
  auto caps = shared_image_stub_->shared_context_state()->GetGLFormatCaps();
  // The target must be GL_TEXTURE_EXTERNAL_OES as the texture is not created
  // with D3D11_BIND_RENDER_TARGET bind flag and so it cannot be bound to the
  // framebuffer. To prevent Skia trying to bind it for read pixels, we need
  // it to be GL_TEXTURE_EXTERNAL_OES.
  std::unique_ptr<gpu::SharedImageBacking> backing =
      gpu::D3DImageBacking::Create(
          mailbox, metadata.format, metadata.size, metadata.color_space,
          metadata.surface_origin, metadata.alpha_type, metadata.usage,
          si_info.debug_label, texture, std::move(dxgi_shared_handle_state),
          caps, GL_TEXTURE_EXTERNAL_OES, array_slice,
          /*use_update_subresource1=*/false,
          /*want_dcomp_texture=*/false, is_thread_safe);
  if (!backing) {
    return nullptr;
  }

  // Need to clear the backing since the D3D11 Video Decoder will initialize
  // the textures.
  backing->SetCleared();
  DCHECK(shared_image_stub_->channel()
             ->gpu_channel_manager()
             ->shared_image_manager());
  shared_image_stub_->factory()->RegisterBacking(std::move(backing));

  return base::WrapRefCounted<ClientSharedImage>(
      new ClientSharedImage(mailbox, si_info, GenVerifiedSyncToken(), holder_,
                            GL_TEXTURE_EXTERNAL_OES));
}
#endif

SharedImageFactory*
GpuChannelSharedImageInterface::GetSharedImageFactoryOnGpuThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!shared_image_stub_) {
    return nullptr;
  }
  return shared_image_stub_->factory();
}

bool GpuChannelSharedImageInterface::MakeContextCurrentOnGpuThread(
    bool needs_gl) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!shared_image_stub_) {
    return false;
  }

  return shared_image_stub_->MakeContextCurrent(needs_gl);
}

void GpuChannelSharedImageInterface::MarkContextLostOnGpuThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  shared_image_stub_->shared_context_state()->MarkContextLost();
}

void GpuChannelSharedImageInterface::ScheduleGpuTask(
    base::OnceClosure task,
    std::vector<SyncToken> sync_token_fences,
    const SyncToken& release) {
  scheduler_->ScheduleTask(gpu::Scheduler::Task(
      sequence_, std::move(task), std::move(sync_token_fences), release));
}

}  // namespace gpu
