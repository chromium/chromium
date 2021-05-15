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
#include "gpu/command_buffer/service/abstract_texture_impl.h"
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
                               GetTextureOwnerMode(),
                               context_state)),
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

void StreamTexture::UpdateAndBindTexImage(GLuint service_id) {
  UpdateTexImage(BindingsMode::kEnsureTexImageBound, service_id);
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

bool StreamTexture::TextureOwnerBindsTextureOnUpdate() {
  DCHECK(texture_owner_);
  return texture_owner_->binds_texture_on_update();
}

void StreamTexture::OnContextLost() {
  texture_owner_ = nullptr;
}

void StreamTexture::UpdateTexImage(BindingsMode bindings_mode,
                                   GLuint service_id) {
  DCHECK(texture_owner_.get());

  if (!has_pending_frame_) {
    // Same frame can be bound multiple times to a different |service_id|. For
    // eg: SharedImageVideo::ProduceGLTexture/ProduceSkia() and hence
    // BeginAccess() can happen multiple times with the same frame by the time
    // next frame is available. In those cases new service_id is generated and
    // the frame although doesn't require any update, still needs to be bound to
    // new service_id.
    EnsureBoundIfNeeded(bindings_mode, service_id);
    return;
  }

  std::unique_ptr<ui::ScopedMakeCurrent> scoped_make_current;
  absl::optional<ScopedRestoreTextureBinding> scoped_restore_texture;
  if (texture_owner_->binds_texture_on_update()) {
    // If the texture_owner() binds the texture while doing the texture update
    // (UpdateTexImage), like in SurfaceTexture case, then make sure that the
    // texture owner's context is made current. This is because the texture
    // which will be bound was generated on TextureOwner's context.
    // For AImageReader case, the texture which will be bound will not
    // necessarily be TextureOwner's texture and hence caller is responsible to
    // handle making correct context current before binding the texture.
    scoped_make_current = MakeCurrent(context_state_.get());

    // If updating the image will implicitly update the texture bindings then
    // restore if requested or the update needed a context switch.
    if (bindings_mode == BindingsMode::kRestoreIfBound ||
        !!scoped_make_current) {
      scoped_restore_texture.emplace();
    }
  }
  texture_owner_->UpdateTexImage();
  EnsureBoundIfNeeded(bindings_mode, service_id);
  has_pending_frame_ = false;
}

void StreamTexture::EnsureBoundIfNeeded(BindingsMode mode, GLuint service_id) {
  DCHECK(texture_owner_);

  if (texture_owner_->binds_texture_on_update()) {
    if (mode == BindingsMode::kEnsureTexImageBound) {
      DCHECK_EQ(service_id, texture_owner_->GetTextureId());
    }
    return;
  }
  if (mode != BindingsMode::kEnsureTexImageBound)
    return;

  DCHECK_GT(service_id, static_cast<unsigned>(0));
  texture_owner_->EnsureTexImageBound(service_id);
}

bool StreamTexture::CopyTexImage(unsigned target) {
  if (target != GL_TEXTURE_EXTERNAL_OES)
    return false;

  if (!texture_owner_.get())
    return false;

  GLint texture_id;
  glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &texture_id);

  // CopyTexImage will only be called for TextureOwner's SurfaceTexture
  // implementation which binds texture to TextureOwner's texture_id on update.
  // Also ensure that the CopyTexImage() is always called on TextureOwner's
  // context.
  DCHECK(texture_owner_->binds_texture_on_update());
  DCHECK(texture_owner_->GetContext()->IsCurrent(nullptr));
  if (texture_id > 0 &&
      static_cast<unsigned>(texture_id) != texture_owner_->GetTextureId())
    return false;

  // On some devices GL_TEXTURE_BINDING_EXTERNAL_OES is not supported as
  // glGetIntegerv() parameter. In this case the value of |texture_id| will be
  // zero and we assume that it is properly bound to TextureOwner's texture id..
  UpdateTexImage(BindingsMode::kEnsureTexImageBound,
                 texture_owner_->GetTextureId());
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

  // We only need TextureOwner to get the latest image and do not require it to
  // be bound to a texture if TextureOwner does not binds texture on update.
  // Hence pass service_id as 0.
  UpdateTexImage(BindingsMode::kRestoreIfBound, /*service_id=*/0);

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
  auto mailbox = gpu::Mailbox::GenerateForSharedImage();

  // TODO(vikassoni): Hardcoding colorspace to SRGB. Figure how if we have a
  // colorspace and wire it here.
  auto shared_image = std::make_unique<SharedImageVideo>(
      mailbox, coded_size, gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, this, context_state_,
      false);
  channel_->shared_image_stub()->factory()->RegisterBacking(
      std::move(shared_image), /*allow_legacy_mailbox=*/false);

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
  // Hence pass service_id as 0.
  UpdateTexImage(BindingsMode::kDontRestoreIfBound, /*service_id=*/0);
  return texture_owner_->GetAHardwareBuffer();
}

}  // namespace gpu
