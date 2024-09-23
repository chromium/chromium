// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/browser_compositor_ios.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/containers/circular_deque.h"
#include "base/lazy_instance.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/features.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_factory.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace content {

////////////////////////////////////////////////////////////////////////////////
// BrowserCompositorIOS

BrowserCompositorIOS::BrowserCompositorIOS(
    gfx::AcceleratedWidget accelerated_widget,
    BrowserCompositorIOSClient* client,
    bool render_widget_host_is_hidden,
    const viz::FrameSinkId& frame_sink_id)
    : client_(client),
      accelerated_widget_(accelerated_widget),
      weak_factory_(this) {
  root_layer_ = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
  // Ensure that this layer draws nothing when it does not not have delegated
  // content (otherwise this solid color will be flashed during navigation).
  root_layer_->SetColor(SK_ColorRED);
  delegated_frame_host_ = std::make_unique<DelegatedFrameHost>(
      frame_sink_id, this, /*should_register_frame_sink_id=*/true);

  SetRenderWidgetHostIsHidden(render_widget_host_is_hidden);
}

BrowserCompositorIOS::~BrowserCompositorIOS() {
  // Ensure that copy callbacks completed or cancelled during further tear-down
  // do not call back into this.
  weak_factory_.InvalidateWeakPtrs();

  TransitionToState(HasNoCompositor);
  delegated_frame_host_.reset();
  root_layer_.reset();
}

DelegatedFrameHost* BrowserCompositorIOS::GetDelegatedFrameHost() {
  DCHECK(delegated_frame_host_);
  return delegated_frame_host_.get();
}

bool BrowserCompositorIOS::ForceNewSurfaceId() {
  dfh_local_surface_id_allocator_.GenerateId();
  delegated_frame_host_->EmbedSurface(
      dfh_local_surface_id_allocator_.GetCurrentLocalSurfaceId(), dfh_size_dip_,
      cc::DeadlinePolicy::UseExistingDeadline());
  return client_->OnBrowserCompositorSurfaceIdChanged();
}

viz::FrameSinkId BrowserCompositorIOS::GetRootFrameSinkId() {
  if (parent_ui_layer_) {
    return parent_ui_layer_->GetCompositor()->frame_sink_id();
  }
  if (compositor_) {
    return compositor_->frame_sink_id();
  }
  return viz::FrameSinkId();
}

void BrowserCompositorIOS::SetBackgroundColor(SkColor background_color) {
  // background_color_ = background_color;
  if (compositor_) {
    compositor_->SetBackgroundColor(background_color_);
  }
}

void BrowserCompositorIOS::UpdateSurfaceFromUIView(
    const gfx::Size& new_size_dip) {
  display::ScreenInfo current = client_->GetCurrentScreenInfo();

  bool is_resize = !dfh_size_dip_.IsEmpty() && new_size_dip != dfh_size_dip_;
  bool needs_new_surface_id =
      new_size_dip != dfh_size_dip_ ||
      current.device_scale_factor != dfh_device_scale_factor_;

  dfh_size_dip_ = new_size_dip;
  dfh_device_scale_factor_ = current.device_scale_factor;

  // The device scale factor is always an integer, so the result here is also
  // an integer.
  dfh_size_pixels_ = gfx::ToRoundedSize(
      gfx::ConvertSizeToPixels(dfh_size_dip_, current.device_scale_factor));
  root_layer_->SetBounds(gfx::Rect(dfh_size_dip_));

  if (needs_new_surface_id) {
    dfh_local_surface_id_allocator_.GenerateId();
    delegated_frame_host_->EmbedSurface(
        dfh_local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
        dfh_size_dip_, GetDeadlinePolicy(is_resize));
  }

  if (compositor_) {
    UpdateSurface(dfh_size_pixels_, current.device_scale_factor,
                  current.display_color_spaces);
  }
}

void BrowserCompositorIOS::UpdateSurfaceFromChild(
    bool auto_resize_enabled,
    float new_device_scale_factor,
    const gfx::Size& new_size_in_pixels,
    const viz::LocalSurfaceId& child_local_surface_id) {
  if (dfh_local_surface_id_allocator_.UpdateFromChild(child_local_surface_id)) {
    if (auto_resize_enabled) {
      client_->SetCurrentDeviceScaleFactor(new_device_scale_factor);
      display::ScreenInfo current = client_->GetCurrentScreenInfo();
      // TODO(danakj): We should avoid lossy conversions to integer DIPs.
      dfh_size_dip_ = gfx::ToFlooredSize(gfx::ConvertSizeToDips(
          new_size_in_pixels, current.device_scale_factor));
      dfh_size_pixels_ = new_size_in_pixels;
      dfh_device_scale_factor_ = new_device_scale_factor;
      root_layer_->SetBounds(gfx::Rect(dfh_size_dip_));
      if (compositor_) {
        UpdateSurface(dfh_size_pixels_, current.device_scale_factor,
                      current.display_color_spaces);
      }
    }
    delegated_frame_host_->EmbedSurface(
        dfh_local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
        dfh_size_dip_, GetDeadlinePolicy(/*is_resize=*/true));
  }
  client_->OnBrowserCompositorSurfaceIdChanged();
}

void BrowserCompositorIOS::UpdateVSyncParameters(
    const base::TimeTicks& timebase,
    const base::TimeDelta& interval) {
  ui::Compositor* compositor = nullptr;
  if (compositor_) {
    compositor = compositor_.get();
  }
  // TODO(ccameron): VSync parameters for a ui::Compositor should be tracked
  // with the owner of that ui::Compositor (which, in the case of MacViews, is
  // BridgedNativeView). For the moment, push the VSync parameters from here to
  // the BridgedNativeView's ui::Compositor because that is a small change and
  // is easy to merge.
  // https://crbug.com/869129
  if (parent_ui_layer_) {
    compositor = parent_ui_layer_->GetCompositor();
  }
  if (compositor) {
    compositor->SetDisplayVSyncParameters(timebase, interval);
  }
}

void BrowserCompositorIOS::SetRenderWidgetHostIsHidden(bool hidden) {
  render_widget_host_is_hidden_ = hidden;
  UpdateState();
  if (state_ == UseParentLayerCompositor) {
    // UpdateState might not call WasShown when showing a frame using the same
    // ParentLayerCompositor, since it returns early on a no-op state
    // transition.
    delegated_frame_host_->WasShown(GetRendererLocalSurfaceId(), dfh_size_dip_,
                                    /*record_tab_switch_time_request=*/{});
  }
}

void BrowserCompositorIOS::SetViewVisible(bool visible) {
  root_layer_->SetVisible(visible);
}

void BrowserCompositorIOS::UpdateState() {
  // Always use the parent ui::Layer's ui::Compositor if available.
  if (parent_ui_layer_) {
    TransitionToState(UseParentLayerCompositor);
    return;
  }

  // If the host is visible and a compositor is required then create one.
  if (!render_widget_host_is_hidden_) {
    TransitionToState(HasOwnCompositor);
    return;
  }

  // Otherwise put the compositor up for recycling.
  TransitionToState(HasNoCompositor);
}

void BrowserCompositorIOS::TransitionToState(State new_state) {
  // Skip if there is no change to make.
  bool is_no_op = false;
  if (state_ == new_state) {
    if (state_ == UseParentLayerCompositor) {
      is_no_op = parent_ui_layer_ == root_layer_->parent();
    } else {
      is_no_op = true;
    }
  }
  if (is_no_op) {
    return;
  }

  // First, detach from the current compositor, if there is one.
  delegated_frame_host_->DetachFromCompositor();
  if (state_ == UseParentLayerCompositor) {
    DCHECK(root_layer_->parent());
    state_ = HasNoCompositor;
    root_layer_->parent()->RemoveObserver(this);
    root_layer_->parent()->Remove(root_layer_.get());
  }
  if (state_ == HasOwnCompositor) {
    compositor_->SetRootLayer(nullptr);
    compositor_.reset();
    InvalidateSurface();
  }

  // The compositor is now detached. If this is the target state, we're done.
  state_ = HasNoCompositor;
  if (new_state == HasNoCompositor) {
    // Don't transiently hide the DelegatedFrameHost because that can cause the
    // current frame to be inappropriately evicted.
    // https://crbug.com/897156
    delegated_frame_host_->WasHidden(DelegatedFrameHost::HiddenCause::kOther);
    return;
  }

  // Attach to the new compositor.
  if (new_state == UseParentLayerCompositor) {
    DCHECK(parent_ui_layer_);
    parent_ui_layer_->Add(root_layer_.get());
    parent_ui_layer_->AddObserver(this);
    state_ = UseParentLayerCompositor;
  }
  if (new_state == HasOwnCompositor) {
    ui::ContextFactory* context_factory = GetContextFactory();
    compositor_ = std::make_unique<ui::Compositor>(
        context_factory->AllocateFrameSinkId(), context_factory,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        ui::IsPixelCanvasRecordingEnabled());
    Suspend();
    display::ScreenInfo current = client_->GetCurrentScreenInfo();
    UpdateSurface(dfh_size_pixels_, current.device_scale_factor,
                  current.display_color_spaces);
    compositor_->SetRootLayer(root_layer_.get());
    compositor_->SetBackgroundColor(background_color_);
    compositor_->SetAcceleratedWidget(accelerated_widget_);
    Unsuspend();
    state_ = HasOwnCompositor;
  }
  DCHECK_EQ(state_, new_state);
  delegated_frame_host_->AttachToCompositor(GetCompositor());
  delegated_frame_host_->WasShown(GetRendererLocalSurfaceId(), dfh_size_dip_,
                                  /*record_tab_switch_time_request=*/{});
}

void BrowserCompositorIOS::TakeFallbackContentFrom(
    BrowserCompositorIOS* other) {
  delegated_frame_host_->TakeFallbackContentFrom(
      other->delegated_frame_host_.get());
}

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost, public:

ui::Layer* BrowserCompositorIOS::DelegatedFrameHostGetLayer() const {
  return root_layer_.get();
}

bool BrowserCompositorIOS::DelegatedFrameHostIsVisible() const {
  return state_ != HasNoCompositor;
}

SkColor BrowserCompositorIOS::DelegatedFrameHostGetGutterColor() const {
  return client_->BrowserCompositorIOSGetGutterColor();
}

void BrowserCompositorIOS::OnFrameTokenChanged(
    uint32_t frame_token,
    base::TimeTicks activation_time) {
  client_->OnFrameTokenChanged(frame_token, activation_time);
}

float BrowserCompositorIOS::GetDeviceScaleFactor() const {
  return dfh_device_scale_factor_;
}

void BrowserCompositorIOS::InvalidateLocalSurfaceIdOnEviction() {
  dfh_local_surface_id_allocator_.Invalidate();
}

viz::FrameEvictorClient::EvictIds
BrowserCompositorIOS::CollectSurfaceIdsForEviction() {
  viz::FrameEvictorClient::EvictIds ids;
  ids.embedded_ids = client_->CollectSurfaceIdsForEviction();
  return ids;
}

bool BrowserCompositorIOS::ShouldShowStaleContentOnEviction() {
  return false;
}

void BrowserCompositorIOS::DidNavigateMainFramePreCommit() {
  delegated_frame_host_->DidNavigateMainFramePreCommit();
}

void BrowserCompositorIOS::DidEnterBackForwardCache() {
  dfh_local_surface_id_allocator_.GenerateId();
  delegated_frame_host_->DidEnterBackForwardCache();
}

void BrowserCompositorIOS::DidNavigate() {
  if (render_widget_host_is_hidden_) {
    // Navigating while hidden should not allocate a new LocalSurfaceID. Once
    // sizes are ready, or we begin to Show, we can then allocate the new
    // LocalSurfaceId.
    dfh_local_surface_id_allocator_.Invalidate();
  } else {
    // The first navigation does not need a new LocalSurfaceID. The renderer can
    // use the ID that was already provided.
    if (!is_first_navigation_) {
      dfh_local_surface_id_allocator_.GenerateId();
    }
    delegated_frame_host_->EmbedSurface(
        dfh_local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
        dfh_size_dip_, cc::DeadlinePolicy::UseExistingDeadline());
    client_->OnBrowserCompositorSurfaceIdChanged();
  }

  delegated_frame_host_->DidNavigate();
  is_first_navigation_ = false;
}

void BrowserCompositorIOS::SetParentUiLayer(ui::Layer* new_parent_ui_layer) {
  if (new_parent_ui_layer) {
    DCHECK(new_parent_ui_layer->GetCompositor());
  }

  // Set |parent_ui_layer_| to the new value, which potentially not match the
  // value of |root_layer_->parent()|. The call to UpdateState will re-parent
  // |root_layer_|.
  DCHECK_EQ(root_layer_->parent(), parent_ui_layer_);
  parent_ui_layer_ = new_parent_ui_layer;
  UpdateState();
  DCHECK_EQ(root_layer_->parent(), parent_ui_layer_);
}

void BrowserCompositorIOS::ForceNewSurfaceForTesting() {
  float current_device_scale_factor =
      client_->GetCurrentScreenInfo().device_scale_factor;
  client_->SetCurrentDeviceScaleFactor(current_device_scale_factor * 2.0f);
  UpdateSurfaceFromUIView(dfh_size_dip_);
}

viz::ScopedSurfaceIdAllocator
BrowserCompositorIOS::GetScopedRendererSurfaceIdAllocator(
    base::OnceCallback<void()> allocation_task) {
  return viz::ScopedSurfaceIdAllocator(&dfh_local_surface_id_allocator_,
                                       std::move(allocation_task));
}

const viz::LocalSurfaceId& BrowserCompositorIOS::GetRendererLocalSurfaceId() {
  if (!dfh_local_surface_id_allocator_.HasValidLocalSurfaceId()) {
    dfh_local_surface_id_allocator_.GenerateId();
  }

  return dfh_local_surface_id_allocator_.GetCurrentLocalSurfaceId();
}

void BrowserCompositorIOS::TransformPointToRootSurface(gfx::PointF* point) {
  gfx::Transform transform_to_root;
  if (parent_ui_layer_) {
    parent_ui_layer_->GetTargetTransformRelativeTo(nullptr, &transform_to_root);
  }
  *point = transform_to_root.MapPoint(*point);
}

void BrowserCompositorIOS::LayerDestroyed(ui::Layer* layer) {
  DCHECK_EQ(layer, parent_ui_layer_);
  SetParentUiLayer(nullptr);
}

ui::Compositor* BrowserCompositorIOS::GetCompositor() const {
  if (parent_ui_layer_) {
    return parent_ui_layer_->GetCompositor();
  }
  return compositor_.get();
}

void BrowserCompositorIOS::InvalidateSurfaceAllocationGroup() {
  local_surface_id_allocator_.Invalidate(
      /*also_invalidate_allocation_group=*/true);
}

cc::DeadlinePolicy BrowserCompositorIOS::GetDeadlinePolicy(
    bool is_resize) const {
  // Determined empirically for smoothness. Don't wait for non-resize frames,
  // as it can cause jank at new tab creation.
  // https://crbug.com/855364
  uint32_t frames_to_wait = is_resize ? 8 : 0;

  // When using the RecyclableCompositor, never wait for frames to arrive
  // (surface sync is managed by the Suspend/Unsuspend lock).
  if (compositor_) {
    frames_to_wait = 0;
  }

  return cc::DeadlinePolicy::UseSpecifiedDeadline(frames_to_wait);
}

void BrowserCompositorIOS::UpdateSurface(
    const gfx::Size& size_pixels,
    float scale_factor,
    const gfx::DisplayColorSpaces& display_color_spaces) {
  if (size_pixels != size_pixels_ || scale_factor != scale_factor_) {
    size_pixels_ = size_pixels;
    scale_factor_ = scale_factor;
    local_surface_id_allocator_.GenerateId();
    viz::LocalSurfaceId local_surface_id =
        local_surface_id_allocator_.GetCurrentLocalSurfaceId();
    compositor_->SetScaleAndSize(scale_factor_, size_pixels_, local_surface_id);
  }
  if (display_color_spaces != display_color_spaces_) {
    display_color_spaces_ = display_color_spaces;
    compositor_->SetDisplayColorSpaces(display_color_spaces_);
  }
}

void BrowserCompositorIOS::InvalidateSurface() {
  size_pixels_ = gfx::Size();
  scale_factor_ = 1.f;
  local_surface_id_allocator_.Invalidate(
      /*also_invalidate_allocation_group=*/true);
}

void BrowserCompositorIOS::Suspend() {
  DCHECK(compositor_);
  // Requests a compositor lock without a timeout.
  compositor_suspended_lock_ =
      compositor_->GetCompositorLock(nullptr, base::TimeDelta());
}

void BrowserCompositorIOS::Unsuspend() {
  compositor_suspended_lock_ = nullptr;
}

}  // namespace content
