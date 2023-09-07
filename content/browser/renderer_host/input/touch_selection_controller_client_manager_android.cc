// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_selection_controller_client_manager_android.h"

#include "content/browser/renderer_host/render_widget_host_view_android.h"

namespace content {

TouchSelectionControllerClientManagerAndroid::
    TouchSelectionControllerClientManagerAndroid(
        RenderWidgetHostViewAndroid* rwhv)
    : rwhv_(rwhv), active_client_(rwhv) {
  DCHECK(rwhv_);
}

TouchSelectionControllerClientManagerAndroid::
    ~TouchSelectionControllerClientManagerAndroid() {
  for (auto& observer : observers_)
    observer.OnManagerWillDestroy(this);
}

// TouchSelectionControllerClientManager implementation.
void TouchSelectionControllerClientManagerAndroid::DidStopFlinging() {
  // TODO(wjmaclean): determine what, if anything, needs to happen here.
}

void TouchSelectionControllerClientManagerAndroid::OnSwipeToMoveCursorBegin() {}

void TouchSelectionControllerClientManagerAndroid::OnSwipeToMoveCursorEnd() {}

void TouchSelectionControllerClientManagerAndroid::OnClientHitTestRegionUpdated(
    ui::TouchSelectionControllerClient* client) {
  if (client != active_client_ || !GetTouchSelectionController() ||
      GetTouchSelectionController()->active_status() ==
          ui::TouchSelectionController::INACTIVE) {
    return;
  }

  active_client_->DidScroll();
}

void TouchSelectionControllerClientManagerAndroid::UpdateClientSelectionBounds(
    const gfx::SelectionBound& start,
    const gfx::SelectionBound& end,
    ui::TouchSelectionControllerClient* client,
    ui::TouchSelectionMenuClient* menu_client) {
  if (client != active_client_ && (!start.HasHandle() || !start.visible()) &&
      (!end.HasHandle() || !end.visible()) &&
      (manager_selection_start_.HasHandle() ||
       manager_selection_end_.HasHandle())) {
    return;
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

void TouchSelectionControllerClientManagerAndroid::ShowContextMenu(
    const gfx::Point& location) {
  active_client_->ShowTouchSelectionContextMenu(location);
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
    const ui::TouchSelectionDraggable::Type type,
    const gfx::PointF& position) {
  rwhv_->OnDragUpdate(type, position);
}

std::unique_ptr<ui::TouchHandleDrawable>
TouchSelectionControllerClientManagerAndroid::CreateDrawable() {
  return rwhv_->CreateDrawable();
}

void TouchSelectionControllerClientManagerAndroid::DidScroll() {
  // Nothing needs to be done here.
}

void TouchSelectionControllerClientManagerAndroid::
    ShowTouchSelectionContextMenu(const gfx::Point& location) {
  active_client_->ShowTouchSelectionContextMenu(location);
}

}  // namespace content
