// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/stream_texture_android.h"

#include <string.h>

#include "base/android/android_image_reader_compat.h"
#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/scheduler_task_runner.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/android_video_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/common/android/scoped_surface_request_conduit.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {
namespace {

std::unique_ptr<ui::ScopedMakeCurrent> MakeCurrent(
    SharedContextState* context_state) {
  std::unique_ptr<ui::ScopedMakeCurrent> scoped_make_current;
  bool needs_make_current =
      !context_state->IsCurrent(nullptr, /*needs_gl=*/true);
  if (needs_make_current) {
    scoped_make_current = std::make_unique<ui::ScopedMakeCurrent>(
        context_state->context(), context_state->surface());
  }
  return scoped_make_current;
}

TextureOwner::Mode GetTextureOwnerMode() {
  return base::android::EnableAndroidImageReader()
             ? TextureOwner::Mode::kAImageReaderInsecure
             : TextureOwner::Mode::kSurfaceTextureInsecure;
}

scoped_refptr<gpu::RefCountedLock> CreateDrDcLockIfNeeded() {
  if (features::NeedThreadSafeAndroidMedia()) {
    return base::MakeRefCounted<gpu::RefCountedLock>();
  }
  return nullptr;
}

}  // namespace

// static
scoped_refptr<StreamTexture> StreamTexture::Create(
    GpuChannel* channel,
    int stream_id,
    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver) {
  ContextResult result;
  auto context_state =
      channel->gpu_channel_manager()->GetSharedContextState(&result);
  if (result != ContextResult::kSuccess)
    return nullptr;
  auto scoped_make_current = MakeCurrent(context_state.get());
  if (scoped_make_current && !scoped_make_current->IsContextCurrent())
    return nullptr;
  return new StreamTexture(channel, stream_id, std::move(receiver),
                           std::move(context_state));
}

// static
void StreamTexture::RunCallback(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::WeakPtr<StreamTexture> weak_stream_texture) {
  if (task_runner->BelongsToCurrentThread()) {
    if (weak_stream_texture)
      weak_stream_texture->OnFrameAvailable();
  } else {
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&StreamTexture::RunCallback, task_runner,
                                  std::move(weak_stream_texture)));
  }
}

StreamTexture::StreamTexture(
    GpuChannel* channel,
    int32_t route_id,
    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver,
    scoped_refptr<SharedContextState> context_state)
    : RefCountedLockHelperDrDc(CreateDrDcLockIfNeeded()),
      texture_owner_(
          TextureOwner::Create(GetTextureOwnerMode(),
                               context_state,
                               GetDrDcLock(),
                               TextureOwnerCodecType::kStreamTexture)),
      has_pending_frame_(false),
      channel_(channel),
      route_id_(route_id),
      context_state_(std::move(context_state)),
      sequence_(channel_->scheduler()->CreateSequence(SchedulingPriority::kLow,
                                                      channel_->task_runner())),
      receiver_(
          this,
          std::move(receiver),
          base::MakeRefCounted<SchedulerTaskRunner>(*channel_->scheduler(),
                                                    sequence_)) {
  channel_->AddRoute(route_id, sequence_);
  texture_owner_->SetFrameAvailableCallback(
      base::BindRepeating(&StreamTexture::RunCallback,
                          base::SingleThreadTaskRunner::GetCurrentDefault(),
                          weak_factory_.GetWeakPtr()));
}

StreamTexture::~StreamTexture() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);

  // |channel_| is always released before GpuChannel releases its reference to
  // this class.
  DCHECK(!channel_);
}

void StreamTexture::ReleaseChannel() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  DCHECK(channel_);
  receiver_.ResetFromAnotherSequenceUnsafe();
  channel_->RemoveRoute(route_id_);
  channel_->scheduler()->DestroySequence(sequence_);
  sequence_ = SequenceId();
  channel_ = nullptr;
}

void StreamTexture::UpdateAndBindTexImage() {}

bool StreamTexture::HasTextureOwner() const {
  return !!texture_owner_;
}

TextureBase* StreamTexture::GetTextureBase() const {
  return texture_owner_->GetTextureBase();
}

void StreamTexture::NotifyOverlayPromotion(bool promotion,
                                           const gfx::Rect& bounds) {}

bool StreamTexture::RenderToOverlay() {
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool StreamTexture::TextureOwnerBindsTextureOnUpdate() {
  DCHECK(texture_owner_);
  return texture_owner_->binds_texture_on_update();
}

void StreamTexture::OnFrameAvailable() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  has_pending_frame_ = true;

  if (!client_ || !texture_owner_ || !channel_)
    return;

  // We haven't received size for first time yet from the MediaPlayer we will
  // defer this sending OnFrameAvailable till then.
  if (rotated_visible_size_.IsEmpty())
    return;

  texture_owner_->UpdateTexImage();
  has_pending_frame_ = false;

  gfx::Rect visible_rect;
  gfx::Size coded_size;
  if (!texture_owner_->GetCodedSizeAndVisibleRect(rotated_visible_size_,
                                                  &coded_size, &visible_rect)) {
    // if we failed to get right size fallback to visible size.
    coded_size = rotated_visible_size_;
    visible_rect = gfx::Rect(coded_size);
  }

  if (coded_size != coded_size_ || visible_rect != visible_rect_) {
    coded_size_ = coded_size;
    visible_rect_ = visible_rect;

    auto mailbox = CreateSharedImage(coded_size);
    viz::VulkanContextProvider* vulkan_context_provider = nullptr;
    DawnContextProvider* dawn_context_provider = nullptr;
    if (context_state_->GrContextIsVulkan()) {
      vulkan_context_provider = context_state_->vk_context_provider();
    } else if (context_state_->IsGraphiteDawnVulkan()) {
      dawn_context_provider = context_state_->dawn_context_provider();
    }
    auto ycbcr_info = AndroidVideoImageBacking::GetYcbcrInfo(
        texture_owner_.get(), vulkan_context_provider, dawn_context_provider);

    client_->OnFrameWithInfoAvailable(mailbox, coded_size, visible_rect,
                                      ycbcr_info);
  } else {
    client_->OnFrameAvailable();
  }
}

void StreamTexture::StartListening(
    mojo::PendingAssociatedRemote<mojom::StreamTextureClient> client) {
  client_.Bind(std::move(client));
}

void StreamTexture::ForwardForSurfaceRequest(
    const base::UnguessableToken& request_token) {
  if (!channel_)
    return;

  ScopedSurfaceRequestConduit::GetInstance()
      ->ForwardSurfaceOwnerForSurfaceRequest(request_token,
                                             texture_owner_.get());
}

gpu::Mailbox StreamTexture::CreateSharedImage(const gfx::Size& coded_size) {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  // We do not update |texture_owner_texture_|'s internal gles2::Texture's
  // size. This is because the gles2::Texture is never used directly, the
  // associated |texture_owner_texture_id_| being the only part of that
  // object we interact with. If we ever use |texture_owner_texture_|, we
  // need to ensure that it gets updated here.

  auto scoped_make_current = MakeCurrent(context_state_.get());
  auto mailbox = gpu::Mailbox::Generate();

  // TODO(vikassoni): Hardcoding colorspace to SRGB. Figure how if we have a
  // colorspace and wire it here.
  auto shared_image = AndroidVideoImageBacking::Create(
      mailbox, coded_size, gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
      /*debug_label=*/"StreamTexture", this, context_state_, GetDrDcLock());
  channel_->shared_image_stub()->factory()->RegisterBacking(
      std::move(shared_image));

  return mailbox;
}

void StreamTexture::UpdateRotatedVisibleSize(
    const gfx::Size& rotated_visible_size) {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  DCHECK(channel_);
  bool was_empty = rotated_visible_size_.IsEmpty();
  rotated_visible_size_ = rotated_visible_size;

  // It's possible that first OnUpdateRotatedVisibleSize will come after first
  // OnFrameAvailable. We delay sending OnFrameWithInfoAvailable if it comes
  // first so now it's time to send it.
  if (was_empty && has_pending_frame_)
    OnFrameAvailable();
}

std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
StreamTexture::GetAHardwareBuffer() {
  DCHECK(texture_owner_);

  return texture_owner_->GetAHardwareBuffer();
}

}  // namespace gpu
