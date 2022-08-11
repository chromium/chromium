// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/dcomp_texture_win.h"

#include <string.h>

#include "base/bind.h"
#include "base/win/windows_types.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/scheduler_task_runner.h"
#include "gpu/command_buffer/service/shared_image/gl_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "ipc/ipc_mojo_bootstrap.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/dcomp_surface_registry.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {

namespace {

constexpr base::TimeDelta kParentWindowPosPollingPeriod =
    base::Milliseconds(1000);

std::unique_ptr<ui::ScopedMakeCurrent> MakeCurrent(
    SharedContextState* context_state) {
  std::unique_ptr<ui::ScopedMakeCurrent> scoped_make_current;
  bool needs_make_current = !context_state->IsCurrent(nullptr);
  if (needs_make_current) {
    scoped_make_current = std::make_unique<ui::ScopedMakeCurrent>(
        context_state->context(), context_state->surface());
  }
  return scoped_make_current;
}

using InitializeGLTextureParams =
    GLTextureImageBackingHelper::InitializeGLTextureParams;

}  // namespace

// static
scoped_refptr<DCOMPTexture> DCOMPTexture::Create(
    GpuChannel* channel,
    int route_id,
    mojo::PendingAssociatedReceiver<mojom::DCOMPTexture> receiver) {
  ContextResult result;
  auto context_state =
      channel->gpu_channel_manager()->GetSharedContextState(&result);
  if (result != ContextResult::kSuccess) {
    DLOG(ERROR) << "GetSharedContextState() failed.";
    return nullptr;
  }

  auto scoped_make_current = MakeCurrent(context_state.get());
  return base::WrapRefCounted(new DCOMPTexture(
      channel, route_id, std::move(receiver), std::move(context_state)));
}

DCOMPTexture::DCOMPTexture(
    GpuChannel* channel,
    int32_t route_id,
    mojo::PendingAssociatedReceiver<mojom::DCOMPTexture> receiver,
    scoped_refptr<SharedContextState> context_state)
    // Size of {1, 1} to signify the Media Foundation rendering pipeline is not
    // ready to setup DCOMP video yet.
    : GLImageDCOMPSurface({1, 1}, INVALID_HANDLE_VALUE),
      channel_(channel),
      route_id_(route_id),
      context_state_(std::move(context_state)),
      sequence_(channel_->scheduler()->CreateSequence(SchedulingPriority::kLow,
                                                      channel_->task_runner())),
      receiver_(this) {
  auto runner = base::MakeRefCounted<SchedulerTaskRunner>(
      *channel_->scheduler(), sequence_);
  IPC::ScopedAllowOffSequenceChannelAssociatedBindings allow_binding;
  receiver_.Bind(std::move(receiver), runner);
  context_state_->AddContextLostObserver(this);
  channel_->AddRoute(route_id, sequence_);
}

DCOMPTexture::~DCOMPTexture() {
  // |channel_| is always released before GpuChannel releases its reference to
  // this class.
  DCHECK(!channel_);

  context_state_->RemoveContextLostObserver(this);
  if (window_pos_timer_.IsRunning()) {
    window_pos_timer_.Stop();
  }
}

void DCOMPTexture::ReleaseChannel() {
  DCHECK(channel_);

  receiver_.ResetFromAnotherSequenceUnsafe();
  channel_->RemoveRoute(route_id_);
  channel_->scheduler()->DestroySequence(sequence_);
  sequence_ = SequenceId();
  channel_ = nullptr;
}

void DCOMPTexture::OnContextLost() {
  context_lost_ = true;
}

bool DCOMPTexture::IsContextValid() const {
  return !context_lost_;
}

void DCOMPTexture::StartListening(
    mojo::PendingAssociatedRemote<mojom::DCOMPTextureClient> client) {
  client_.Bind(std::move(client));
}

void DCOMPTexture::SetTextureSize(const gfx::Size& size) {
  SetSize(size);
  if (!shared_image_mailbox_created_) {
    if (client_) {
      shared_image_mailbox_created_ = true;
      gpu::Mailbox mailbox = CreateSharedImage();
      client_->OnSharedImageMailboxBound(mailbox);
    } else
      DLOG(ERROR) << "Unable to call client_->OnSharedImageMailboxBound";
  }
}

void DCOMPTexture::SetDCOMPSurfaceHandle(
    const base::UnguessableToken& token,
    SetDCOMPSurfaceHandleCallback callback) {
  DVLOG(1) << __func__;

  base::win::ScopedHandle surface_handle =
      gl::DCOMPSurfaceRegistry::GetInstance()->TakeDCOMPSurfaceHandle(token);
  if (!surface_handle.IsValid()) {
    DLOG(ERROR) << __func__ << ": No surface registered for token " << token;
    std::move(callback).Run(false);
    return;
  }

  surface_handle_.Set(surface_handle.Take());
  std::move(callback).Run(true);
}

gpu::Mailbox DCOMPTexture::CreateSharedImage() {
  DCHECK(channel_);

  auto scoped_make_current = MakeCurrent(context_state_.get());
  auto mailbox = gpu::Mailbox::GenerateForSharedImage();

  // Use GLImageBacking as the backing to hold GLImageDCOMPSurface
  // and be able to retrieve it later via ProduceOverlay.
  // Use some reasonable defaults for params to create GLImageBacking
  // since params are only used when the backing is accessed for GL.
  // Note: this backing shouldn't be accessed via GL at all.
  InitializeGLTextureParams params;
  params.target = GL_TEXTURE_2D;
  params.internal_format = GL_RGBA;
  params.format = GL_RGBA;
  params.is_cleared = true;
  params.is_rgb_emulation = false;
  params.framebuffer_attachment_angle = false;
  auto shared_image = std::make_unique<GLImageBacking>(
      this, mailbox, viz::BGRA_8888, GetSize(), gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
      /*usage=*/SHARED_IMAGE_USAGE_DISPLAY, params,
      /*use_passthrough=*/true);

  channel_->shared_image_stub()->factory()->RegisterBacking(
      std::move(shared_image), /*allow_legacy_mailbox=*/false);

  return mailbox;
}

gfx::Rect DCOMPTexture::GetParentWindowRect() {
  RECT parent_window_rect = {};
  ::GetWindowRect(last_parent_, &parent_window_rect);
  return gfx::Rect(parent_window_rect);
}

void DCOMPTexture::OnUpdateParentWindowRect() {
  gfx::Rect parent_window_rect = GetParentWindowRect();
  if (parent_window_rect_ != parent_window_rect) {
    parent_window_rect_ = parent_window_rect;
    SendOutputRect();
  }
}

void DCOMPTexture::SetParentWindow(HWND parent) {
  GLImageDCOMPSurface::SetParentWindow(parent);
  OnUpdateParentWindowRect();
  if (!window_pos_timer_.IsRunning()) {
    window_pos_timer_.Start(FROM_HERE, kParentWindowPosPollingPeriod, this,
                            &DCOMPTexture::OnUpdateParentWindowRect);
  }
}

void DCOMPTexture::SetRect(const gfx::Rect& window_relative_rect) {
  bool should_send_output_rect = false;
  if (window_relative_rect != window_relative_rect_) {
    window_relative_rect_ = window_relative_rect;
    should_send_output_rect = true;
  }

  gfx::Rect parent_window_rect = GetParentWindowRect();
  if (parent_window_rect_ != parent_window_rect) {
    parent_window_rect_ = parent_window_rect;
    should_send_output_rect = true;
  }

  if (should_send_output_rect)
    SendOutputRect();
}

void DCOMPTexture::SendOutputRect() {
  if (!client_)
    return;

  gfx::Rect output_rect = window_relative_rect_;
  output_rect.set_x(window_relative_rect_.x() + parent_window_rect_.x());
  output_rect.set_y(window_relative_rect_.y() + parent_window_rect_.y());
  if (last_output_rect_ != output_rect) {
    if (!output_rect.IsEmpty()) {
      // The initial `OnUpdateParentWindowRect()` call can cause an empty
      // `output_rect`.
      // Set MFMediaEngine's `UpdateVideoStream()` with an non-empty destination
      // rectangle. Otherwise, the next `EnableWindowlessSwapchainMode()` call
      // to MFMediaEngine will skip the creation of the DCOMP surface handle.
      // Then, the next `GetVideoSwapchainHandle()` call returns S_FALSE.
      client_->OnOutputRectChange(output_rect);
    }
    last_output_rect_ = output_rect;
  }
}

}  // namespace gpu
