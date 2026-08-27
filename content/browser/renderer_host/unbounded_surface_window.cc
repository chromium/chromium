// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/unbounded_surface_window.h"

#include <type_traits>

#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
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

template <typename EventType>
void RouteWebPointerEvent(RenderWidgetHostViewBase* parent_view,
                          const EventType& event) {
  if (!parent_view || !parent_view->host() ||
      !parent_view->host()->delegate() ||
      !parent_view->host()->delegate()->GetInputEventRouter()) {
    return;
  }
  RenderWidgetHostViewBase* root_view =
      static_cast<RenderWidgetHostViewBase*>(parent_view->GetRootView());
  if (!root_view) {
    return;
  }

  EventType web_event = event;
  gfx::PointF screen_point(web_event.PositionInScreen());
  gfx::Point root_origin = root_view->GetViewBounds().origin();
  gfx::PointF root_point =
      screen_point - gfx::Vector2dF(root_origin.x(), root_origin.y());
  gfx::PointF parent_local_point =
      parent_view->TransformRootPointToViewCoordSpace(root_point);
  web_event.SetPositionInWidget(parent_local_point.x(), parent_local_point.y());

  if constexpr (std::is_same_v<EventType, blink::WebMouseEvent>) {
    parent_view->host()->delegate()->GetInputEventRouter()->RouteMouseEvent(
        parent_view, &web_event, ui::LatencyInfo());
  } else if constexpr (std::is_same_v<EventType, blink::WebMouseWheelEvent>) {
    parent_view->host()
        ->delegate()
        ->GetInputEventRouter()
        ->RouteMouseWheelEvent(parent_view, &web_event, ui::LatencyInfo());
  }
}

}  // namespace

UnboundedSurfaceWindow::UnboundedSurfaceWindow() = default;
UnboundedSurfaceWindow::~UnboundedSurfaceWindow() = default;

void UnboundedSurfaceWindow::RouteMouseEvent(
    const blink::WebMouseEvent& event) {
  RouteWebPointerEvent(GetParentView(), event);
}

void UnboundedSurfaceWindow::RouteMouseWheelEvent(
    const blink::WebMouseWheelEvent& event) {
  RouteWebPointerEvent(GetParentView(), event);
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
