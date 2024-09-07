// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/dcomp_texture_win.h"

#include <string.h>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/power_monitor/power_monitor.h"
#include "base/win/windows_types.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/scheduler_task_runner.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "ipc/ipc_mojo_bootstrap.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/video_types.h"
#include "ui/gl/dcomp_surface_registry.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {

namespace {

constexpr base::TimeDelta kParentWindowPosPollingPeriod = base::Seconds(1);
constexpr base::TimeDelta kPowerChangeDetectionGracePeriod = base::Seconds(2);

class DCOMPTextureRepresentation : public OverlayImageRepresentation {
 public:
  DCOMPTextureRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<gl::DCOMPSurfaceProxy> dcomp_surface_proxy)
      : OverlayImageRepresentation(manager, backing, tracker),
        dcomp_surface_proxy_(std::move(dcomp_surface_proxy)) {}

  std::optional<gl::DCLayerOverlayImage> GetDCLayerOverlayImage() override {
    return std::make_optional<gl::DCLayerOverlayImage>(size(),
                                                       dcomp_surface_proxy_);
  }

  bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) override {
    return true;
  }

  void EndReadAccess(gfx::GpuFenceHandle release_fence) override {}

 private:
  scoped_refptr<gl::DCOMPSurfaceProxy> dcomp_surface_proxy_;
};

class DCOMPTextureBacking : public ClearTrackingSharedImageBacking {
 public:
  DCOMPTextureBacking(scoped_refptr<gl::DCOMPSurfaceProxy> dcomp_surface_proxy,
                      const Mailbox& mailbox,
                      const gfx::Size& size)
      : ClearTrackingSharedImageBacking(mailbox,
                                        viz::SinglePlaneFormat::kBGRA_8888,
                                        size,
                                        gfx::ColorSpace::CreateSRGB(),
                                        kTopLeft_GrSurfaceOrigin,
                                        kPremul_SkAlphaType,
                                        gpu::SHARED_IMAGE_USAGE_SCANOUT,
                                        {},
                                        /*estimated_size=*/0,
                                        /*is_thread_safe=*/false),
        dcomp_surface_proxy_(std::move(dcomp_surface_proxy)) {
    SetCleared();
  }

  SharedImageBackingType GetType() const override {
    return SharedImageBackingType::kDCOMPSurfaceProxy;
  }

  std::unique_ptr<OverlayImageRepresentation> ProduceOverlay(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override {
    return std::make_unique<DCOMPTextureRepresentation>(manager, this, tracker,
                                                        dcomp_surface_proxy_);
  }

 private:
  scoped_refptr<gl::DCOMPSurfaceProxy> dcomp_surface_proxy_;
};

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
  return base::WrapRefCounted(new DCOMPTexture(
      channel, route_id, std::move(receiver), std::move(context_state)));
}

DCOMPTexture::DCOMPTexture(
    GpuChannel* channel,
    int32_t route_id,
    mojo::PendingAssociatedReceiver<mojom::DCOMPTexture> receiver,
    scoped_refptr<SharedContextState> context_state)
    : channel_(channel),
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
  base::PowerMonitor::GetInstance()->AddPowerSuspendObserver(this);
  channel_->AddRoute(route_id, sequence_);
}

DCOMPTexture::~DCOMPTexture() {
  DVLOG(1) << __func__;
  // |channel_| is always released before GpuChannel releases its reference to
  // this class.
  DCHECK(!channel_);

  context_state_->RemoveContextLostObserver(this);
  base::PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);

  if (window_pos_timer_.IsRunning()) {
    window_pos_timer_.Stop();
  }
}

void DCOMPTexture::ReleaseChannel() {
  DVLOG(1) << __func__;
  DCHECK(channel_);

  receiver_.ResetFromAnotherSequenceUnsafe();
  channel_->RemoveRoute(route_id_);
  channel_->scheduler()->DestroySequence(sequence_);
  sequence_ = SequenceId();
  channel_ = nullptr;

  ResetSizeIfNeeded();
}

void DCOMPTexture::OnContextLost() {
  DVLOG(1) << __func__;
}

// TODO(xhwang): Also observe GPU LUID change.
void DCOMPTexture::OnResume() {
  DVLOG(1) << __func__;
  last_power_change_time_ = base::TimeTicks::Now();
  ResetSizeIfNeeded();
}

void DCOMPTexture::ResetSizeIfNeeded() {
  DVLOG(2) << __func__;
  // For `kHardwareProtected` video frame, when hardware content reset happens,
  // e.g. OS suspend/resume or GPU hot swap, existing video frames become stale
  // and presenting them could cause issues like black screen flash (see
  // crbug.com/1384544). So we set `size_` to (1, 1) so that DComp surface
  // resources will be released (see SwapChainPresenter::PresentDCOMPSurface()).
  // We don't know for sure whether hardware content reset happened. So we check
  // whether power suspend/resume or GPU change happened recently as a hint.
  // Since it's a hint, to prevent breaking normal playback, we only do this
  // when the video frame is orphaned (the media Renderer has been suspended or
  // destroyed, but we are still showing the last frame), which will trigger
  // `ReleaseChannel()` and set `channel_` to null.
  if (!channel_ &&
      protected_video_type_ == gfx::ProtectedVideoType::kHardwareProtected &&
      base::TimeTicks::Now() - last_power_change_time_ <
          kPowerChangeDetectionGracePeriod) {
    DVLOG(1) << __func__
             << ": Resetting size to {1,1} to release dcomp surface resources "
                "and prevent stale content from being displayed";
    size_ = gfx::Size(1, 1);
  }
}

void DCOMPTexture::StartListening(
    mojo::PendingAssociatedRemote<mojom::DCOMPTextureClient> client) {
  client_.Bind(std::move(client));
}

void DCOMPTexture::SetTextureSize(const gfx::Size& size) {
  size_ = size;
  if (!shared_image_mailbox_created_) {
    if (client_) {
      shared_image_mailbox_created_ = true;
      gpu::Mailbox mailbox = CreateSharedImage();
      client_->OnSharedImageMailboxBound(mailbox);
    } else
      DLOG(ERROR) << "Unable to call client_->OnSharedImageMailboxBound";
  }
}

const gfx::Size& DCOMPTexture::GetSize() const {
  return size_;
}

HANDLE DCOMPTexture::GetSurfaceHandle() {
  return surface_handle_.get();
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

  auto mailbox = gpu::Mailbox::Generate();

  // Use DCOMPTextureBacking as the backing to hold DCOMPSurfaceProxy i.e. this,
  // and be able to retrieve it later via ProduceOverlay.
  // Note: DCOMPTextureBacking shouldn't be accessed via GL at all.
  auto shared_image =
      std::make_unique<DCOMPTextureBacking>(this, mailbox, size_);

  channel_->shared_image_stub()->factory()->RegisterBacking(
      std::move(shared_image));

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
  if (last_parent_ != parent) {
    last_parent_ = parent;
    OnUpdateParentWindowRect();
    if (!window_pos_timer_.IsRunning()) {
      window_pos_timer_.Start(FROM_HERE, kParentWindowPosPollingPeriod, this,
                              &DCOMPTexture::OnUpdateParentWindowRect);
    }
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

void DCOMPTexture::SetProtectedVideoType(
    gfx::ProtectedVideoType protected_video_type) {
  if (protected_video_type == protected_video_type_)
    return;

  DVLOG(2) << __func__ << ": protected_video_type="
           << static_cast<int>(protected_video_type);
  protected_video_type_ = protected_video_type;
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
