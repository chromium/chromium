// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_selection_controller_client_manager_android.h"

#include "components/viz/common/hit_test/aggregated_hit_test_region.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"

namespace content {

TouchSelectionControllerClientManagerAndroid::
    TouchSelectionControllerClientManagerAndroid(
        RenderWidgetHostViewAndroid* rwhv,
        viz::HostFrameSinkManager* host_frame_sink_manager)
    : rwhv_(rwhv),
      host_frame_sink_manager_(host_frame_sink_manager),
      active_client_(rwhv) {
  DCHECK(rwhv_);
  DCHECK(host_frame_sink_manager_);
}

TouchSelectionControllerClientManagerAndroid::
    ~TouchSelectionControllerClientManagerAndroid() {
  if (active_client_ != rwhv_)
    host_frame_sink_manager_->RemoveHitTestRegionObserver(this);

  for (auto& observer : observers_)
    observer.OnManagerWillDestroy(this);
}

// TouchSelectionControllerClientManager implementation.
void TouchSelectionControllerClientManagerAndroid::DidStopFlinging() {
  // TODO(wjmaclean): determine what, if anything, needs to happen here.
}

void TouchSelectionControllerClientManagerAndroid::UpdateClientSelectionBounds(
    const gfx::SelectionBound& start,
    const gfx::SelectionBound& end,
    ui::TouchSelectionControllerClient* client,
    ui::TouchSelectionMenuClient* menu_client) {
  if (client != active_client_ &&
      (start.type() == gfx::SelectionBound::EMPTY || !start.visible()) &&
      (end.type() == gfx::SelectionBound::EMPTY || !end.visible()) &&
      (manager_selection_start_.type() != gfx::SelectionBound::EMPTY ||
       manager_selection_end_.type() != gfx::SelectionBound::EMPTY)) {
    return;
  }

  // Since the observer method does very little processing, and early-outs when
  // not displaying handles, we don't bother un-installing it when an OOPIF
  // client is not currently displaying handles.
  if (client != active_client_) {
    if (active_client_ == rwhv_)  // We are switching to an OOPIF client.
      host_frame_sink_manager_->AddHitTestRegionObserver(this);
    else if (client == rwhv_)  // We are switching to a non-OOPIF client.
      host_frame_sink_manager_->RemoveHitTestRegionObserver(this);
  }

  active_client_ = client;
  manager_selection_start_ = start;
  manager_selection_end_ = end;

  // Notify TouchSelectionController if anything should change here. Only
  // update if the client is different and not making a change to empty, or
  // is the same client.
  if (GetTouchSelectionController()) {
    GetTouchSelectionController()->OnSelectionBoundsChanged(
        manager_selection_start_, manager_selection_end_);
  }
}

void TouchSelectionControllerClientManagerAndroid::InvalidateClient(
    ui::TouchSelectionControllerClient* client) {
  if (active_client_ == client) {
    if (active_client_ != rwhv_)
      host_frame_sink_manager_->RemoveHitTestRegionObserver(this);
    active_client_ = rwhv_;
  }
}

ui::TouchSelectionController*
TouchSelectionControllerClientManagerAndroid::GetTouchSelectionController() {
  return rwhv_->touch_selection_controller();
}

void TouchSelectionControllerClientManagerAndroid::AddObserver(
    Observer* observer) {
  observers_.AddObserver(observer);
}

void TouchSelectionControllerClientManagerAndroid::RemoveObserver(
    Observer* observer) {
  observers_.RemoveObserver(observer);
}

// TouchSelectionControllerClient implementation.
bool TouchSelectionControllerClientManagerAndroid::SupportsAnimation() const {
  return rwhv_->SupportsAnimation();
}

void TouchSelectionControllerClientManagerAndroid::SetNeedsAnimate() {
  rwhv_->SetNeedsAnimate();
}

void TouchSelectionControllerClientManagerAndroid::MoveCaret(
    const gfx::PointF& position) {
  active_client_->MoveCaret(position);
}

void TouchSelectionControllerClientManagerAndroid::MoveRangeSelectionExtent(
    const gfx::PointF& extent) {
  active_client_->MoveRangeSelectionExtent(extent);
}

void TouchSelectionControllerClientManagerAndroid::SelectBetweenCoordinates(
    const gfx::PointF& base,
    const gfx::PointF& extent) {
  active_client_->SelectBetweenCoordinates(base, extent);
}

void TouchSelectionControllerClientManagerAndroid::OnSelectionEvent(
    ui::SelectionEventType event) {
  // Always defer to the top-level RWHV TSC for this.
  rwhv_->OnSelectionEvent(event);
}

void TouchSelectionControllerClientManagerAndroid::OnDragUpdate(
    const gfx::PointF& position) {
  rwhv_->OnDragUpdate(position);
}

std::unique_ptr<ui::TouchHandleDrawable>
TouchSelectionControllerClientManagerAndroid::CreateDrawable() {
  return rwhv_->CreateDrawable();
}

void TouchSelectionControllerClientManagerAndroid::DidScroll() {
  // Nothing needs to be done here.
}

void TouchSelectionControllerClientManagerAndroid::
    OnAggregatedHitTestRegionListUpdated(
        const viz::FrameSinkId& frame_sink_id,
        const std::vector<viz::AggregatedHitTestRegion>& hit_test_data) {
  DCHECK(active_client_ != rwhv_);

  if (!GetTouchSelectionController() ||
      GetTouchSelectionController()->active_status() ==
          ui::TouchSelectionController::INACTIVE) {
    return;
  }

  active_client_->DidScroll();
}

}  // namespace content
