// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/unbounded_surface_window.h"

#include <type_traits>

#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "ui/latency/latency_info.h"

namespace content {

namespace {

// Fallback timer duration to ensure the unbounded surface is destroyed if the
// renderer fails to respond.
constexpr base::TimeDelta kDismissFallbackTimeout = base::Seconds(10);

}  // namespace

// static
std::optional<gfx::PointF>
UnboundedSurfaceWindow::TransformScreenPointToViewCoordSpace(
    RenderWidgetHostViewBase* target_view,
    const gfx::PointF& screen_point) {
  if (!target_view) {
    return std::nullopt;
  }
  auto* root_view =
      static_cast<RenderWidgetHostViewBase*>(target_view->GetRootView());
  if (!root_view) {
    return std::nullopt;
  }
  gfx::Point root_origin = root_view->GetViewBounds().origin();
  gfx::PointF root_point =
      screen_point - gfx::Vector2dF(root_origin.x(), root_origin.y());
  gfx::PointF local_point;
  if (!root_view->TransformPointToCoordSpaceForView(root_point, target_view,
                                                    &local_point)) {
    return std::nullopt;
  }
  return local_point;
}

// static
bool UnboundedSurfaceWindow::TransformEventCoordinatesToView(
    RenderWidgetHostViewBase* target_view,
    blink::WebMouseEvent& web_event) {
  std::optional<gfx::PointF> local_point = TransformScreenPointToViewCoordSpace(
      target_view, web_event.PositionInScreen());
  if (!local_point) {
    return false;
  }
  web_event.SetPositionInWidget(local_point->x(), local_point->y());
  return true;
}

UnboundedSurfaceWindow::UnboundedSurfaceWindow(
    base::WeakPtr<RenderWidgetHostViewBase> parent_view,
    base::WeakPtr<RenderWidgetHostViewBase> subframe_view)
    : parent_view_(std::move(parent_view)),
      subframe_view_(std::move(subframe_view)) {}

UnboundedSurfaceWindow::~UnboundedSurfaceWindow() = default;

RenderWidgetHostViewBase* UnboundedSurfaceWindow::GetTargetView() const {
  return subframe_view_ ? subframe_view_.get() : parent_view_.get();
}

template <typename EventType>
RenderWidgetHostViewBase* UnboundedSurfaceWindow::PrepareEventForTargetView(
    EventType& web_event) {
  RenderWidgetHostViewBase* target_view = GetTargetView();
  if (!target_view || !target_view->host()) {
    return nullptr;
  }
  if (!TransformEventCoordinatesToView(target_view, web_event)) {
    return nullptr;
  }
  return target_view;
}

void UnboundedSurfaceWindow::RouteMouseEvent(
    const blink::WebMouseEvent& event) {
  blink::WebMouseEvent web_event = event;
  if (RenderWidgetHostViewBase* target_view =
          PrepareEventForTargetView(web_event)) {
    target_view->ProcessMouseEvent(web_event, ui::LatencyInfo());
  }
}

void UnboundedSurfaceWindow::RouteMouseWheelEvent(
    const blink::WebMouseWheelEvent& event) {
  blink::WebMouseWheelEvent web_event = event;
  if (RenderWidgetHostViewBase* target_view =
          PrepareEventForTargetView(web_event)) {
    target_view->ProcessMouseWheelEvent(web_event, ui::LatencyInfo());
  }
}

void UnboundedSurfaceWindow::Dismiss() {
  if (!IsValid() || dismiss_pending_) {
    return;
  }
  dismiss_pending_ = true;

  if (client_remote_.is_bound()) {
    client_remote_->OnDismissed();
  }

  dismiss_fallback_timer_.Start(
      FROM_HERE, kDismissFallbackTimeout,
      base::BindOnce(&UnboundedSurfaceWindow::ScheduleDeferredDestroy,
                     base::Unretained(this)));
}

void UnboundedSurfaceWindow::DidPresentFrameAfterDismissal() {
  if (!dismiss_pending_) {
    return;
  }
  dismiss_fallback_timer_.Stop();
  ScheduleDeferredDestroy();
}

void UnboundedSurfaceWindow::DidCancelDismissal() {
  if (!dismiss_pending_) {
    return;
  }
  dismiss_pending_ = false;
  dismiss_fallback_timer_.Stop();
}

void UnboundedSurfaceWindow::ScheduleDeferredDestroy() {
  // Destruction is deferred to a posted task so that callers in active call
  // stacks (e.g. Mojo message dispatchers or timer callbacks) can return safely
  // before the window and its associated platform objects are destroyed.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&UnboundedSurfaceWindow::DestroyInternal, GetWeakPtr()));
}

void UnboundedSurfaceWindow::DestroyInternal() {
  if (!dismiss_pending_) {
    return;
  }
  client_remote_.reset();
  TeardownAndDestroy();
}

}  // namespace content
