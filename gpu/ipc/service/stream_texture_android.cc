// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/stream_texture_android.h"

#include <string.h>

#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/abstract_texture_impl_shared_context_state.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_video.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/common/android/scoped_surface_request_conduit.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_messages.h"
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
  return features::IsAImageReaderEnabled()
             ? TextureOwner::Mode::kAImageReaderInsecure
             : TextureOwner::Mode::kSurfaceTextureInsecure;
}

}  // namespace

// static
scoped_refptr<StreamTexture> StreamTexture::Create(GpuChannel* channel,
                                                   int stream_id) {
  ContextResult result;
  auto context_state =
      channel->gpu_channel_manager()->GetSharedContextState(&result);
  if (result != ContextResult::kSuccess)
    return nullptr;
  auto scoped_make_current = MakeCurrent(context_state.get());
  return new StreamTexture(channel, stream_id, std::move(context_state));
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

StreamTexture::StreamTexture(GpuChannel* channel,
                             int32_t route_id,
                             scoped_refptr<SharedContextState> context_state)
    : texture_owner_(
          TextureOwner::Create(TextureOwner::CreateTexture(context_state),
                               GetTextureOwnerMode())),
      has_pending_frame_(false),
      channel_(channel),
      route_id_(route_id),
      has_listener_(false),
      context_state_(std::move(context_state)),
      sequence_(
          channel_->scheduler()->CreateSequence(SchedulingPriority::kLow)),
      sync_point_client_state_(
          channel_->sync_point_manager()->CreateSyncPointClientState(
              CommandBufferNamespace::GPU_IO,
              CommandBufferIdFromChannelAndRoute(channel_->client_id(),
                                                 route_id),
              sequence_)) {
  context_state_->AddContextLostObserver(this);
  channel->AddRoute(route_id, sequence_, this);

  texture_owner_->SetFrameAvailableCallback(base::BindRepeating(
      &StreamTexture::RunCallback, base::ThreadTaskRunnerHandle::Get(),
      weak_factory_.GetWeakPtr()));
}

StreamTexture::~StreamTexture() {
  // |channel_| is always released before GpuChannel releases its reference to
  // this class.
  DCHECK(!channel_);
  context_state_->RemoveContextLostObserver(this);
}

void StreamTexture::ReleaseChannel() {
  DCHECK(channel_);
  channel_->RemoveRoute(route_id_);
  channel_->scheduler()->DestroySequence(sequence_);
  sequence_ = SequenceId();
  sync_point_client_state_->Destroy();
  sync_point_client_state_ = nullptr;
  channel_ = nullptr;
}

bool StreamTexture::IsUsingGpuMemory() const {
  // Once the image is bound during the first update, we just replace/update the
  // same image every time in future and hence the image is always bound to a
  // texture. This means that it always uses gpu memory.
  return true;
}

void StreamTexture::UpdateAndBindTexImage() {
  UpdateTexImage(BindingsMode::kEnsureTexImageBound);
}

bool StreamTexture::HasTextureOwner() const {
  return !!texture_owner_;
}

TextureBase* StreamTexture::GetTextureBase() const {
  return texture_owner_->GetTextureBase();
}

void StreamTexture::NotifyOverlayPromotion(bool promotion,
                                           const gfx::Rect& bounds) {}

bool StreamTexture::RenderToOverlay() {
  NOTREACHED();
  return false;
}

void StreamTexture::OnContextLost() {
  texture_owner_ = nullptr;
}

void StreamTexture::UpdateTexImage(BindingsMode bindings_mode) {
  DCHECK(texture_owner_.get());

  if (!has_pending_frame_) return;

  auto scoped_make_current = MakeCurrent(context_state_.get());

  // We also restore the previous binding even if the previous binding is same
  // as the one which we are going to bind. This could be little inefficient.
  // TODO(vikassoni): Update logic similar to what CodecImage does to optimize.
  gl::ScopedTextureBinder scoped_bind_texture(GL_TEXTURE_EXTERNAL_OES,
                                              texture_owner_->GetTextureId());
  texture_owner_->UpdateTexImage();
  EnsureBoundIfNeeded(bindings_mode);
  has_pending_frame_ = false;
}

void StreamTexture::EnsureBoundIfNeeded(BindingsMode mode) {
  DCHECK(texture_owner_);

  if (texture_owner_->binds_texture_on_update())
    return;
  if (mode != BindingsMode::kEnsureTexImageBound)
    return;
  texture_owner_->EnsureTexImageBound();
}

bool StreamTexture::CopyTexImage(unsigned target) {
  if (target != GL_TEXTURE_EXTERNAL_OES)
    return false;

  if (!texture_owner_.get())
    return false;

  GLint texture_id;
  glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &texture_id);

  // The following code only works if we're being asked to copy into
  // |texture_id_|. Copying into a different texture is not supported.
  // On some devices GL_TEXTURE_BINDING_EXTERNAL_OES is not supported as
  // glGetIntegerv() parameter. In this case the value of |texture_id| will be
  // zero and we assume that it is properly bound to |texture_id_|.
  if (texture_id > 0 &&
      static_cast<unsigned>(texture_id) != texture_owner_->GetTextureId())
    return false;

  UpdateTexImage(BindingsMode::kEnsureTexImageBound);

  return true;
}

void StreamTexture::OnFrameAvailable() {
  has_pending_frame_ = true;

  if (!has_listener_ || !channel_ || !texture_owner_)
    return;

  // We haven't received size for first time yet from the MediaPlayer we will
  // defer this sending OnFrameAvailable till then.
  if (rotated_visible_size_.IsEmpty())
    return;

  UpdateTexImage(BindingsMode::kEnsureTexImageBound);

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
    auto ycbcr_info =
        SharedImageVideo::GetYcbcrInfo(texture_owner_.get(), context_state_);

    channel_->Send(new GpuStreamTextureMsg_FrameWithInfoAvailable(
        route_id_, mailbox, coded_size, visible_rect, ycbcr_info));
  } else {
    channel_->Send(new GpuStreamTextureMsg_FrameAvailable(route_id_));
  }
}

gfx::Size StreamTexture::GetSize() {
  return coded_size_;
}

unsigned StreamTexture::GetInternalFormat() {
  return GL_RGBA;
}

unsigned StreamTexture::GetDataType() {
  return GL_UNSIGNED_BYTE;
}

bool StreamTexture::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(StreamTexture, message)
    IPC_MESSAGE_HANDLER(GpuStreamTextureMsg_StartListening, OnStartListening)
    IPC_MESSAGE_HANDLER(GpuStreamTextureMsg_ForwardForSurfaceRequest,
                        OnForwardForSurfaceRequest)
    IPC_MESSAGE_HANDLER(GpuStreamTextureMsg_UpdateRotatedVisibleSize,
                        OnUpdateRotatedVisibleSize)
    IPC_MESSAGE_HANDLER(GpuStreamTextureMsg_Destroy, OnDestroy)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  DCHECK(handled);
  return handled;
}

void StreamTexture::OnStartListening() {
  DCHECK(!has_listener_);
  has_listener_ = true;
}

void StreamTexture::OnForwardForSurfaceRequest(
    const base::UnguessableToken& request_token) {
  if (!channel_)
    return;

  ScopedSurfaceRequestConduit::GetInstance()
      ->ForwardSurfaceOwnerForSurfaceRequest(request_token,
                                             texture_owner_.get());
}

gpu::Mailbox StreamTexture::CreateSharedImage(const gfx::Size& coded_size) {
  // We do not update |texture_owner_texture_|'s internal gles2::Texture's
  // size. This is because the gles2::Texture is never used directly, the
  // associated |texture_owner_texture_id_| being the only part of that
  // object we interact with. If we ever use |texture_owner_texture_|, we
  // need to ensure that it gets updated here.

  auto scoped_make_current = MakeCurrent(context_state_.get());

  bool use_passthrough =
      context_state_->feature_info()->is_passthrough_cmd_decoder();
  std::unique_ptr<gles2::AbstractTexture> legacy_mailbox_texture;
  if (use_passthrough) {
    legacy_mailbox_texture =
        std::make_unique<gles2::AbstractTextureImplOnSharedContextPassthrough>(
            GL_TEXTURE_EXTERNAL_OES, context_state_);
  } else {
    legacy_mailbox_texture =
        std::make_unique<gles2::AbstractTextureImplOnSharedContext>(
            GL_TEXTURE_EXTERNAL_OES, GL_RGBA, coded_size.width(),
            coded_size.height(), 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
            context_state_);
  }
  legacy_mailbox_texture->BindStreamTextureImage(
      this, texture_owner_->GetTextureId());

  auto mailbox = gpu::Mailbox::GenerateForSharedImage();

  // TODO(vikassoni): Hardcoding colorspace to SRGB. Figure how if we have a
  // colorspace and wire it here.
  auto shared_image = std::make_unique<SharedImageVideo>(
      mailbox, coded_size, gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, this,
      std::move(legacy_mailbox_texture), context_state_, false);
  channel_->shared_image_stub()->factory()->RegisterBacking(
      std::move(shared_image), true /* allow_legacy_mailbox */);

  return mailbox;
}

void StreamTexture::OnUpdateRotatedVisibleSize(
    const gfx::Size& rotated_visible_size) {
  DCHECK(channel_);
  bool was_empty = rotated_visible_size_.IsEmpty();
  rotated_visible_size_ = rotated_visible_size;

  // It's possible that first OnUpdateRotatedVisibleSize will come after first
  // OnFrameAvailable. We delay sending OnFrameWithInfoAvailable if it comes
  // first so now it's time to send it.
  if (was_empty && has_pending_frame_)
    OnFrameAvailable();
}

void StreamTexture::OnDestroy() {
  DCHECK(channel_);

  // The following call may delete the StreamTexture, so we must ensure that no
  // access to |this| occurs after the call.
  channel_->DestroyStreamTexture(route_id_);
}

StreamTexture::BindOrCopy StreamTexture::ShouldBindOrCopy() {
  return COPY;
}

bool StreamTexture::BindTexImage(unsigned target) {
  NOTREACHED();
  return false;
}

void StreamTexture::ReleaseTexImage(unsigned target) {
}

bool StreamTexture::CopyTexSubImage(unsigned target,
                                    const gfx::Point& offset,
                                    const gfx::Rect& rect) {
  return false;
}

bool StreamTexture::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    int z_order,
    gfx::OverlayTransform transform,
    const gfx::Rect& bounds_rect,
    const gfx::RectF& crop_rect,
    bool enable_blend,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  NOTREACHED();
  return false;
}

void StreamTexture::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                                 uint64_t process_tracing_id,
                                 const std::string& dump_name) {
  // TODO(ericrk): Add OnMemoryDump for GLImages. crbug.com/514914
}

bool StreamTexture::HasMutableState() const {
  return false;
}

std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
StreamTexture::GetAHardwareBuffer() {
  DCHECK(texture_owner_);

  // Using BindingsMode::kDontRestoreIfBound here since we do not want to bind
  // the image. We just want to get the AHardwareBuffer from the latest image.
  UpdateTexImage(BindingsMode::kDontRestoreIfBound);
  return texture_owner_->GetAHardwareBuffer();
}

}  // namespace gpu
