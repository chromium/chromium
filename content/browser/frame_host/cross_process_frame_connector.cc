// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/cross_process_frame_connector.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "components/viz/common/features.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_host_manager.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/cursor_manager.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/common/frame_messages.h"
#include "content/public/common/screen_info.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/gfx/geometry/dip_util.h"

namespace content {

namespace {

// If |frame| is an iframe or a GuestView, returns its parent, null otherwise.
RenderFrameHostImpl* ParentRenderFrameHost(RenderFrameHostImpl* frame) {
  // Find the parent in the FrameTree (iframe).
  if (frame->GetParent())
    return frame->GetParent();

  // Find the parent in the WebContentsTree (GuestView).
  FrameTreeNode* frame_in_embedder =
      frame->frame_tree_node()->render_manager()->GetOuterDelegateNode();
  if (frame_in_embedder)
    return frame_in_embedder->current_frame_host()->GetParent();

  // No parent found.
  return nullptr;
}

// Return the root RenderFrameHost in the outermost WebContents.
RenderFrameHostImpl* RootRenderFrameHost(RenderFrameHostImpl* frame) {
  RenderFrameHostImpl* current = frame;
  while (true) {
    RenderFrameHostImpl* parent = ParentRenderFrameHost(current);
    if (!parent)
      return current;
    current = parent;
  };
}

}  // namespace

CrossProcessFrameConnector::CrossProcessFrameConnector(
    RenderFrameProxyHost* frame_proxy_in_parent_renderer)
    : FrameConnectorDelegate(IsUseZoomForDSFEnabled()),
      frame_proxy_in_parent_renderer_(frame_proxy_in_parent_renderer) {
  frame_proxy_in_parent_renderer->frame_tree_node()
      ->render_manager()
      ->current_frame_host()
      ->GetRenderWidgetHost()
      ->GetScreenInfo(&screen_info_);
}

CrossProcessFrameConnector::~CrossProcessFrameConnector() {
  if (!IsVisible()) {
    // MaybeLogCrash will check 1) if there was a crash or not and 2) if the
    // crash might have been already logged earlier as kCrashedWhileVisible or
    // kShownAfterCrashing.
    MaybeLogCrash(CrashVisibility::kNeverVisibleAfterCrash);
  }

  // Notify the view of this object being destroyed, if the view still exists.
  SetView(nullptr);
}

bool CrossProcessFrameConnector::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(CrossProcessFrameConnector, msg)
    IPC_MESSAGE_HANDLER(FrameHostMsg_SynchronizeVisualProperties,
                        OnSynchronizeVisualProperties)
    IPC_MESSAGE_HANDLER(FrameHostMsg_UpdateViewportIntersection,
                        OnUpdateViewportIntersection)
    IPC_MESSAGE_HANDLER(FrameHostMsg_SetIsInert, OnSetIsInert)
    IPC_MESSAGE_HANDLER(FrameHostMsg_UpdateRenderThrottlingStatus,
                        OnUpdateRenderThrottlingStatus)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void CrossProcessFrameConnector::SetView(RenderWidgetHostViewChildFrame* view) {
  // Detach ourselves from the previous |view_|.
  if (view_) {
    RenderWidgetHostViewBase* root_view = GetRootRenderWidgetHostView();
    if (root_view && root_view->GetCursorManager())
      root_view->GetCursorManager()->ViewBeingDestroyed(view_);

    // The RenderWidgetHostDelegate needs to be checked because SetView() can
    // be called during nested WebContents destruction. See
    // https://crbug.com/644306.
    if (GetParentRenderWidgetHostView() &&
        GetParentRenderWidgetHostView()->host()->delegate() &&
        GetParentRenderWidgetHostView()
            ->host()
            ->delegate()
            ->GetInputEventRouter()) {
      GetParentRenderWidgetHostView()
          ->host()
          ->delegate()
          ->GetInputEventRouter()
          ->WillDetachChildView(view_);
    }
    view_->SetFrameConnectorDelegate(nullptr);
  }

  ResetScreenSpaceRect();
  view_ = view;

  // Attach ourselves to the new view and size it appropriately. Also update
  // visibility in case the frame owner is hidden in parent process. We should
  // try to move these updates to a single IPC (see https://crbug.com/750179).
  if (view_) {
    if (has_crashed_ && !IsVisible()) {
      // MaybeLogCrash will check 1) if there was a crash or not and 2) if the
      // crash might have been already logged earlier as kCrashedWhileVisible or
      // kShownAfterCrashing.
      MaybeLogCrash(CrashVisibility::kNeverVisibleAfterCrash);
    }
    is_crash_already_logged_ = has_crashed_ = false;
    delegate_was_shown_after_crash_ = false;

    view_->SetFrameConnectorDelegate(this);
    if (visibility_ != blink::mojom::FrameVisibility::kRenderedInViewport)
      OnVisibilityChanged(visibility_);
    FrameMsg_ViewChanged_Params params;
    params.frame_sink_id = view_->GetFrameSinkId();
    frame_proxy_in_parent_renderer_->Send(new FrameMsg_ViewChanged(
        frame_proxy_in_parent_renderer_->GetRoutingID(), params));
  }
}

void CrossProcessFrameConnector::RenderProcessGone() {
  has_crashed_ = true;

  FrameTreeNode* node = frame_proxy_in_parent_renderer_->frame_tree_node();
  int process_id = node->current_frame_host()->GetProcess()->GetID();
  for (node = node->parent(); node; node = node->parent()) {
    if (node->current_frame_host()->GetProcess()->GetID() == process_id) {
      // The crash will be already logged by the ancestor - ignore this crash in
      // the current instance of the CrossProcessFrameConnector.
      is_crash_already_logged_ = true;
    }
  }

  if (IsVisible())
    MaybeLogCrash(CrashVisibility::kCrashedWhileVisible);

  frame_proxy_in_parent_renderer_->Send(new FrameMsg_ChildFrameProcessGone(
      frame_proxy_in_parent_renderer_->GetRoutingID()));

  auto* parent_view = GetParentRenderWidgetHostView();
  if (parent_view && parent_view->host()->delegate())
    parent_view->host()->delegate()->SubframeCrashed(visibility_);
}

void CrossProcessFrameConnector::SendIntrinsicSizingInfoToParent(
    const blink::WebIntrinsicSizingInfo& sizing_info) {
  frame_proxy_in_parent_renderer_->Send(
      new FrameMsg_IntrinsicSizingInfoOfChildChanged(
          frame_proxy_in_parent_renderer_->GetRoutingID(), sizing_info));
}

void CrossProcessFrameConnector::UpdateCursor(const WebCursor& cursor) {
  RenderWidgetHostViewBase* root_view = GetRootRenderWidgetHostView();
  // UpdateCursor messages are ignored if the root view does not support
  // cursors.
  if (root_view && root_view->GetCursorManager())
    root_view->GetCursorManager()->UpdateCursor(view_, cursor);
}

gfx::PointF CrossProcessFrameConnector::TransformPointToRootCoordSpace(
    const gfx::PointF& point,
    const viz::SurfaceId& surface_id) {
  gfx::PointF transformed_point;
  TransformPointToCoordSpaceForView(point, GetRootRenderWidgetHostView(),
                                    surface_id, &transformed_point);
  return transformed_point;
}

bool CrossProcessFrameConnector::TransformPointToCoordSpaceForView(
    const gfx::PointF& point,
    RenderWidgetHostViewBase* target_view,
    const viz::SurfaceId& local_surface_id,
    gfx::PointF* transformed_point) {
  RenderWidgetHostViewBase* root_view = GetRootRenderWidgetHostView();
  if (!root_view)
    return false;

  // It is possible that neither the original surface or target surface is an
  // ancestor of the other in the RenderWidgetHostView tree (e.g. they could
  // be siblings). To account for this, the point is first transformed into the
  // root coordinate space and then the root is asked to perform the conversion.
  if (!root_view->TransformPointToLocalCoordSpace(point, local_surface_id,
                                                  transformed_point))
    return false;

  if (target_view == root_view)
    return true;

  return root_view->TransformPointToCoordSpaceForView(
      *transformed_point, target_view, transformed_point);
}

void CrossProcessFrameConnector::ForwardAckedTouchpadZoomEvent(
    const blink::WebGestureEvent& event,
    InputEventAckState ack_result) {
  auto* root_view = GetRootRenderWidgetHostView();
  if (!root_view)
    return;

  blink::WebGestureEvent root_event(event);
  const gfx::PointF root_point =
      view_->TransformPointToRootCoordSpaceF(event.PositionInWidget());
  root_event.SetPositionInWidget(root_point);
  root_view->GestureEventAck(root_event, ack_result);
}

bool CrossProcessFrameConnector::BubbleScrollEvent(
    const blink::WebGestureEvent& event) {
  DCHECK(event.GetType() == blink::WebInputEvent::kGestureScrollBegin ||
         event.GetType() == blink::WebInputEvent::kGestureScrollUpdate ||
         event.GetType() == blink::WebInputEvent::kGestureScrollEnd);
  auto* parent_view = GetParentRenderWidgetHostView();

  if (!parent_view)
    return false;

  auto* event_router = parent_view->host()->delegate()->GetInputEventRouter();

  // We will only convert the coordinates back to the root here. The
  // RenderWidgetHostInputEventRouter will determine which ancestor view will
  // receive a resent gesture event, so it will be responsible for converting to
  // the coordinates of the target view.
  blink::WebGestureEvent resent_gesture_event(event);
  const gfx::PointF root_point =
      view_->TransformPointToRootCoordSpaceF(event.PositionInWidget());
  resent_gesture_event.SetPositionInWidget(root_point);
  // When a gesture event is bubbled to the parent frame, set the allowed touch
  // action of the parent frame to Auto so that this gesture event is allowed.
  parent_view->host()->input_router()->ForceSetTouchActionAuto();

  return event_router->BubbleScrollEvent(parent_view, view_,
                                         resent_gesture_event);
}

bool CrossProcessFrameConnector::HasFocus() {
  RenderWidgetHostViewBase* root_view = GetRootRenderWidgetHostView();
  if (root_view)
    return root_view->HasFocus();
  return false;
}

void CrossProcessFrameConnector::FocusRootView() {
  RenderWidgetHostViewBase* root_view = GetRootRenderWidgetHostView();
  if (root_view)
    root_view->Focus();
}

bool CrossProcessFrameConnector::LockMouse(bool request_unadjusted_movement) {
  RenderWidgetHostViewBase* root_view = GetRootRenderWidgetHostView();
  if (root_view)
    return root_view->LockMouse(request_unadjusted_movement);
  return false;
}

void CrossProcessFrameConnector::UnlockMouse() {
  RenderWidgetHostViewBase* root_view = GetRootRenderWidgetHostView();
  if (root_view)
    root_view->UnlockMouse();
}

void CrossProcessFrameConnector::OnSynchronizeVisualProperties(
    const viz::FrameSinkId& frame_sink_id,
    const FrameVisualProperties& visual_properties) {
  TRACE_EVENT_WITH_FLOW2(
      TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
      "CrossProcessFrameConnector::OnSynchronizeVisualProperties Receive "
      "Message",
      TRACE_ID_GLOBAL(
          visual_properties.local_surface_id_allocation.local_surface_id()
              .submission_trace_id()),
      TRACE_EVENT_FLAG_FLOW_IN, "message",
      "FrameHostMsg_SynchronizeVisualProperties", "new_local_surface_id",
      visual_properties.local_surface_id_allocation.local_surface_id()
          .ToString());
  // If the |screen_space_rect| or |screen_info| of the frame has changed, then
  // the viz::LocalSurfaceId must also change.
  if ((last_received_local_frame_size_ != visual_properties.local_frame_size ||
       screen_info_ != visual_properties.screen_info ||
       capture_sequence_number() != visual_properties.capture_sequence_number ||
       last_received_zoom_level_ != visual_properties.zoom_level) &&
      local_surface_id_allocation_.local_surface_id() ==
          visual_properties.local_surface_id_allocation.local_surface_id()) {
    bad_message::ReceivedBadMessage(
        frame_proxy_in_parent_renderer_->GetProcess(),
        bad_message::CPFC_RESIZE_PARAMS_CHANGED_LOCAL_SURFACE_ID_UNCHANGED);
    return;
  }

  last_received_zoom_level_ = visual_properties.zoom_level;
  last_received_local_frame_size_ = visual_properties.local_frame_size;
  SynchronizeVisualProperties(frame_sink_id, visual_properties);
}

void CrossProcessFrameConnector::OnUpdateViewportIntersection(
    const blink::ViewportIntersectionState& intersection_state) {
  intersection_state_ = intersection_state;
  if (view_)
    view_->UpdateViewportIntersection(intersection_state);

  if (IsVisible()) {
    // Record metrics if a crashed subframe became visible as a result of this
    // viewport intersection update.  For example, this might happen if a user
    // scrolls to a crashed subframe.
    MaybeLogShownCrash(ShownAfterCrashingReason::kViewportIntersection);
  }
}

void CrossProcessFrameConnector::OnVisibilityChanged(
    blink::mojom::FrameVisibility visibility) {
  bool visible = visibility != blink::mojom::FrameVisibility::kNotRendered;
  visibility_ = visibility;
  if (IsVisible()) {
    // Record metrics if a crashed subframe became visible as a result of this
    // visibility change.
    MaybeLogShownCrash(ShownAfterCrashingReason::kVisibility);
  }
  if (!view_)
    return;

  frame_proxy_in_parent_renderer_->frame_tree_node()
      ->current_frame_host()
      ->VisibilityChanged(visibility);

  // If there is an inner WebContents, it should be notified of the change in
  // the visibility. The Show/Hide methods will not be called if an inner
  // WebContents exists since the corresponding WebContents will itself call
  // Show/Hide on all the RenderWidgetHostViews (including this) one.
  if (frame_proxy_in_parent_renderer_->frame_tree_node()
          ->render_manager()
          ->IsMainFrameForInnerDelegate()) {
    view_->host()->delegate()->OnRenderFrameProxyVisibilityChanged(visibility_);
    return;
  }

  if (visible && !view_->host()->delegate()->IsHidden()) {
    view_->Show();
  } else if (!visible) {
    view_->Hide();
  }
}

void CrossProcessFrameConnector::OnSetIsInert(bool inert) {
  is_inert_ = inert;
  if (view_)
    view_->SetIsInert();
}

void CrossProcessFrameConnector::OnSetInheritedEffectiveTouchAction(
    cc::TouchAction touch_action) {
  inherited_effective_touch_action_ = touch_action;
  if (view_)
    view_->UpdateInheritedEffectiveTouchAction();
}

RenderWidgetHostViewBase*
CrossProcessFrameConnector::GetRootRenderWidgetHostView() {
  // Tests may not have frame_proxy_in_parent_renderer_ set.
  if (!frame_proxy_in_parent_renderer_)
    return nullptr;

  RenderFrameHostImpl* current =
      frame_proxy_in_parent_renderer_->frame_tree_node()->current_frame_host();
  RenderFrameHostImpl* root = RootRenderFrameHost(current);
  return static_cast<RenderWidgetHostViewBase*>(root->GetView());
}

RenderWidgetHostViewBase*
CrossProcessFrameConnector::GetParentRenderWidgetHostView() {
  RenderFrameHostImpl* current =
      frame_proxy_in_parent_renderer_->frame_tree_node()->current_frame_host();
  RenderFrameHostImpl* parent = ParentRenderFrameHost(current);
  return parent ? static_cast<RenderWidgetHostViewBase*>(parent->GetView())
                : nullptr;
}

void CrossProcessFrameConnector::EnableAutoResize(const gfx::Size& min_size,
                                                  const gfx::Size& max_size) {
  frame_proxy_in_parent_renderer_->Send(new FrameMsg_EnableAutoResize(
      frame_proxy_in_parent_renderer_->GetRoutingID(), min_size, max_size));
}

void CrossProcessFrameConnector::DisableAutoResize() {
  frame_proxy_in_parent_renderer_->Send(new FrameMsg_DisableAutoResize(
      frame_proxy_in_parent_renderer_->GetRoutingID()));
}

bool CrossProcessFrameConnector::IsInert() const {
  return is_inert_;
}

cc::TouchAction CrossProcessFrameConnector::InheritedEffectiveTouchAction()
    const {
  return inherited_effective_touch_action_;
}

bool CrossProcessFrameConnector::IsHidden() const {
  return visibility_ == blink::mojom::FrameVisibility::kNotRendered;
}

void CrossProcessFrameConnector::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {
  frame_proxy_in_parent_renderer_->Send(new FrameMsg_DidUpdateVisualProperties(
      frame_proxy_in_parent_renderer_->GetRoutingID(), metadata));
}

void CrossProcessFrameConnector::SetVisibilityForChildViews(
    bool visible) const {
  frame_proxy_in_parent_renderer_->frame_tree_node()
      ->current_frame_host()
      ->SetVisibilityForChildViews(visible);
}

void CrossProcessFrameConnector::SetScreenSpaceRect(
    const gfx::Rect& screen_space_rect) {
  gfx::Rect old_rect = screen_space_rect_in_pixels_;
  FrameConnectorDelegate::SetScreenSpaceRect(screen_space_rect);

  if (view_) {
    view_->SetBounds(screen_space_rect_in_dip_);

    // Other local root frames nested underneath this one implicitly have their
    // view rects changed when their ancestor is repositioned, and therefore
    // need to have their screen rects updated.
    FrameTreeNode* proxy_node =
        frame_proxy_in_parent_renderer_->frame_tree_node();
    if (old_rect.x() != screen_space_rect_in_pixels_.x() ||
        old_rect.y() != screen_space_rect_in_pixels_.y()) {
      for (FrameTreeNode* node :
           proxy_node->frame_tree()->SubtreeNodes(proxy_node)) {
        if (node != proxy_node && node->current_frame_host()->is_local_root())
          node->current_frame_host()->GetRenderWidgetHost()->SendScreenRects();
      }
    }
  }
}

void CrossProcessFrameConnector::ResetScreenSpaceRect() {
  local_surface_id_allocation_ = viz::LocalSurfaceIdAllocation();
  // TODO(lfg): Why do we need to reset the screen_space_rect_ that comes from
  // the parent when setting the child? https://crbug.com/809275
  screen_space_rect_in_pixels_ = gfx::Rect();
  screen_space_rect_in_dip_ = gfx::Rect();
  last_received_local_frame_size_ = gfx::Size();
}

void CrossProcessFrameConnector::OnUpdateRenderThrottlingStatus(
    bool is_throttled,
    bool subtree_throttled) {
  if (is_throttled != is_throttled_ ||
      subtree_throttled != subtree_throttled_) {
    is_throttled_ = is_throttled;
    subtree_throttled_ = subtree_throttled;
    if (view_)
      view_->UpdateRenderThrottlingStatus();
  }
}

bool CrossProcessFrameConnector::IsThrottled() const {
  return is_throttled_;
}

bool CrossProcessFrameConnector::IsSubtreeThrottled() const {
  return subtree_throttled_;
}

bool CrossProcessFrameConnector::MaybeLogCrash(CrashVisibility visibility) {
  if (!has_crashed_)
    return false;

  // Only log once per renderer crash.
  if (is_crash_already_logged_)
    return false;
  is_crash_already_logged_ = true;

  // Actually log the UMA.
  UMA_HISTOGRAM_ENUMERATION("Stability.ChildFrameCrash.Visibility", visibility);

  return true;
}

void CrossProcessFrameConnector::MaybeLogShownCrash(
    ShownAfterCrashingReason reason) {
  if (!MaybeLogCrash(CrashVisibility::kShownAfterCrashing))
    return;

  // Identify cases where the sad frame was initially in a hidden tab, then the
  // tab became visible, and finally the sad frame became visible because it
  // was scrolled into view or its visibility changed.  Record these cases
  // separately, since they might be avoided by reloading the tab when it
  // becomes visible.
  if (delegate_was_shown_after_crash_) {
    if (reason == ShownAfterCrashingReason::kViewportIntersection)
      reason = ShownAfterCrashingReason::kViewportIntersectionAfterTabWasShown;
    else if (reason == ShownAfterCrashingReason::kVisibility)
      reason = ShownAfterCrashingReason::kVisibilityAfterTabWasShown;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "Stability.ChildFrameCrash.ShownAfterCrashingReason", reason);
}

void CrossProcessFrameConnector::DelegateWasShown() {
  if (IsVisible()) {
    // MaybeLogShownCrash will check 1) if there was a crash or not and 2) if
    // the crash might have been already logged earlier as
    // kCrashedWhileVisible.
    MaybeLogShownCrash(
        CrossProcessFrameConnector::ShownAfterCrashingReason::kTabWasShown);
  }

  if (has_crashed_)
    delegate_was_shown_after_crash_ = true;
}

bool CrossProcessFrameConnector::IsVisible() {
  if (visibility_ == blink::mojom::FrameVisibility::kNotRendered)
    return false;
  if (intersection_state().viewport_intersection.IsEmpty())
    return false;

  Visibility embedder_visibility =
      frame_proxy_in_parent_renderer_->frame_tree_node()
          ->current_frame_host()
          ->delegate()
          ->GetVisibility();
  if (embedder_visibility != Visibility::VISIBLE)
    return false;

  return true;
}

}  // namespace content
