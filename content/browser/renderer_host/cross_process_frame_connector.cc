// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/cross_process_frame_connector.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/optional_trace_event.h"
#include "components/input/cursor_manager.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/viz/common/features.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_manager.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/common/features.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/geometry/dip_util.h"

namespace content {

CrossProcessFrameConnector::CrossProcessFrameConnector(
    RenderFrameProxyHost* frame_proxy_in_parent_renderer)
    : frame_proxy_in_parent_renderer_(frame_proxy_in_parent_renderer) {
  // Skip for tests.
  if (!frame_proxy_in_parent_renderer_) {
    screen_infos_ = display::ScreenInfos(display::ScreenInfo());
    return;
  }

  // At this point, SetView() has not been called and so the associated
  // RenderWidgetHost doesn't have a view yet. That means calling
  // GetScreenInfos() on the associated RenderWidgetHost will just default to
  // the primary display, which may not be appropriate. So instead we call
  // GetScreenInfos() on the root RenderWidgetHost, which will be guaranteed to
  // be on the correct display. All subsequent updates to |screen_infos_|
  // ultimately come from the root, so it makes sense to do it here as well.
  screen_infos_ = current_child_frame_host()
                      ->GetOutermostMainFrameOrEmbedder()
                      ->GetRenderWidgetHost()
                      ->GetScreenInfos();
}

CrossProcessFrameConnector::~CrossProcessFrameConnector() {
  if (!IsVisible()) {
    // MaybeLogCrash will check 1) if there was a crash or not and 2) if the
    // crash might have been already logged earlier as kCrashedWhileVisible or
    // kShownAfterCrashing.
    MaybeLogCrash(CrashVisibility::kNeverVisibleAfterCrash);
  }

  // Notify the view of this object being destroyed, if the view still exists.
  SetView(nullptr, /*allow_paint_holding=*/false);
}

void CrossProcessFrameConnector::SetView(RenderWidgetHostViewChildFrame* view,
                                         bool allow_paint_holding) {
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
    view_->SetFrameConnector(nullptr);
  }

  ResetRectInParentView();
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

    view_->SetFrameConnector(this);
    if (visibility_ != blink::mojom::FrameVisibility::kRenderedInViewport)
      OnVisibilityChanged(visibility_);
    if (frame_proxy_in_parent_renderer_ &&
        frame_proxy_in_parent_renderer_->is_render_frame_proxy_live()) {
      frame_proxy_in_parent_renderer_->GetAssociatedRemoteFrame()
          ->SetFrameSinkId(view_->GetFrameSinkId(), allow_paint_holding);
    }
  }
}

void CrossProcessFrameConnector::RenderProcessGone() {
  OPTIONAL_TRACE_EVENT1("content",
                        "CrossProcessFrameConnector::RenderProcessGone",
                        "visibility", visibility_);
  has_crashed_ = true;

  RenderFrameHostImpl* current_child_rfh = current_child_frame_host();
  int process_id = current_child_rfh->GetProcess()->GetDeprecatedID();

  // If a parent, outer document or embedder of `current_child_rfh` has crashed
  // and has the same RPH, we only want to record the crash once.
  for (auto* rfh = current_child_rfh->GetParentOrOuterDocumentOrEmbedder(); rfh;
       rfh = rfh->GetParentOrOuterDocumentOrEmbedder()) {
    if (rfh->GetProcess()->GetDeprecatedID() == process_id) {
      // The crash will be already logged by the ancestor - ignore this crash in
      // the current instance of the CrossProcessFrameConnector.
      is_crash_already_logged_ = true;
    }
  }

  if (IsVisible())
    MaybeLogCrash(CrashVisibility::kCrashedWhileVisible);

  frame_proxy_in_parent_renderer_->ChildProcessGone();

  // The following call might discard the WebContents by
  // DiscardPageWithCrashedSubframePolicy, which in turn calls
  // NavigationController::SetNeedsReload(). It is safe to potentially call
  // SetNeedsReload() again below, as it is a lightweight operation that just
  // overwrites the type of the reload needed, which only affects metrics.
  GetContentClient()->browser()->CrossProcessSubframeRenderProcessGone(
      current_child_rfh);

  if (current_child_rfh->delegate()) {
    // If a subframe crashed on a hidden tab, mark the tab for reload to avoid
    // showing a sad frame to the user if they ever switch back to that tab. Do
    // this for subframes that are either visible in viewport or visible but
    // scrolled out of view, but skip subframes that are not rendered (e.g., via
    // "display:none"), since in that case the user wouldn't see a sad frame
    // anyway. Prerendering subframes do not enter this code since
    // RenderFrameHostImpl immediately cancels prerender if a render process
    // exits. We only mark the tab for reload for active subframes to exclude
    // cases like crashed frames in the back/forward cache.
    bool did_mark_for_reload = false;
    if (current_child_rfh->delegate()->GetVisibility() != Visibility::VISIBLE &&
        visibility_ != blink::mojom::FrameVisibility::kNotRendered &&
        base::FeatureList::IsEnabled(
            features::kReloadHiddenTabsWithCrashedSubframes) &&
        current_child_rfh->IsActive()) {
      frame_proxy_in_parent_renderer_->frame_tree_node()
          ->frame_tree()
          .controller()
          .SetNeedsReload(
              NavigationControllerImpl::NeedsReloadType::kCrashedSubframe);
      did_mark_for_reload = true;
      UMA_HISTOGRAM_ENUMERATION(
          "Stability.ChildFrameCrash.TabMarkedForReload.Visibility",
          visibility_);
    }

    UMA_HISTOGRAM_BOOLEAN("Stability.ChildFrameCrash.TabMarkedForReload",
                          did_mark_for_reload);
  }
}

void CrossProcessFrameConnector::SendIntrinsicSizingInfoToParent(
    blink::mojom::IntrinsicSizingInfoPtr sizing_info) {
  // The width/height should not be negative since gfx::SizeF will clamp
  // negative values to zero.
  DCHECK((sizing_info->size.width() >= 0.f) &&
         (sizing_info->size.height() >= 0.f));
  DCHECK((sizing_info->aspect_ratio.width() >= 0.f) &&
         (sizing_info->aspect_ratio.height() >= 0.f));
  if (!frame_proxy_in_parent_renderer_->is_render_frame_proxy_live())
    return;
  frame_proxy_in_parent_renderer_->GetAssociatedRemoteFrame()
      ->IntrinsicSizingInfoOfChildChanged(std::move(sizing_info));
}

void CrossProcessFrameConnector::SynchronizeVisualProperties(
    const blink::FrameVisualProperties& visual_properties,
    bool propagate) {
  last_received_zoom_level_ = visual_properties.zoom_level;
  last_received_css_zoom_factor_ = visual_properties.css_zoom_factor;
  last_received_local_frame_size_ = visual_properties.local_frame_size;
  screen_infos_ = visual_properties.screen_infos;
  local_surface_id_ = visual_properties.local_surface_id;

  capture_sequence_number_ = visual_properties.capture_sequence_number;

  SetRectInParentView(visual_properties.rect_in_local_root);
  SetLocalFrameSize(visual_properties.local_frame_size);

  if (!view_)
    return;

  view_->UpdateScreenInfo();

  RenderWidgetHostImpl* render_widget_host = view_->host();
  DCHECK(render_widget_host);

  render_widget_host->SetAutoResize(visual_properties.auto_resize_enabled,
                                    visual_properties.min_size_for_auto_resize,
                                    visual_properties.max_size_for_auto_resize);
  render_widget_host->SetVisualPropertiesFromParentFrame(
      visual_properties.page_scale_factor,
      visual_properties.compositing_scale_factor,
      visual_properties.is_pinch_gesture_active,
      visual_properties.visible_viewport_size,
      visual_properties.compositor_viewport,
      visual_properties.root_widget_viewport_segments);

  render_widget_host->UpdateVisualProperties(propagate);
}

input::RenderWidgetHostViewInput*
CrossProcessFrameConnector::GetParentViewInput() {
  return GetParentRenderWidgetHostView();
}

input::RenderWidgetHostViewInput*
CrossProcessFrameConnector::GetRootViewInput() {
  return GetRootRenderWidgetHostView();
}

void CrossProcessFrameConnector::UpdateCursor(const ui::Cursor& cursor) {
  RenderWidgetHostViewBase* root_view = GetRootRenderWidgetHostView();
  // UpdateCursor messages are ignored if the root view does not support
  // cursors.
  if (root_view && root_view->GetCursorManager())
    root_view->GetCursorManager()->UpdateCursor(view_, cursor);
}

CrossProcessFrameConnector::RootViewFocusState
CrossProcessFrameConnector::HasFocus() {
  RenderWidgetHostViewBase* root_view = GetRootRenderWidgetHostView();
  if (!root_view) {
    return RootViewFocusState::kNullView;
  }
  return root_view->HasFocus() ? RootViewFocusState::kFocused
                               : RootViewFocusState::kNotFocused;
}

void CrossProcessFrameConnector::FocusRootView() {
  RenderWidgetHostViewBase* root_view = GetRootRenderWidgetHostView();
  if (root_view)
    root_view->Focus();
}

blink::mojom::PointerLockResult CrossProcessFrameConnector::LockPointer(
    bool request_unadjusted_movement) {
  RenderWidgetHostViewBase* root_view = GetRootRenderWidgetHostView();
  if (root_view)
    return root_view->LockPointer(request_unadjusted_movement);
  return blink::mojom::PointerLockResult::kWrongDocument;
}

blink::mojom::PointerLockResult CrossProcessFrameConnector::ChangePointerLock(
    bool request_unadjusted_movement) {
  RenderWidgetHostViewBase* root_view = GetRootRenderWidgetHostView();
  if (root_view)
    return root_view->ChangePointerLock(request_unadjusted_movement);
  return blink::mojom::PointerLockResult::kWrongDocument;
}

void CrossProcessFrameConnector::UnlockPointer() {
  RenderWidgetHostViewBase* root_view = GetRootRenderWidgetHostView();
  if (root_view)
    root_view->UnlockPointer();
}

void CrossProcessFrameConnector::OnSynchronizeVisualProperties(
    const blink::FrameVisualProperties& visual_properties) {
  TRACE_EVENT_WITH_FLOW2(
      TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
      "CrossProcessFrameConnector::OnSynchronizeVisualProperties Receive "
      "Message",
      TRACE_ID_GLOBAL(visual_properties.local_surface_id.submission_trace_id()),
      TRACE_EVENT_FLAG_FLOW_IN, "message",
      "FrameHostMsg_SynchronizeVisualProperties", "new_local_surface_id",
      visual_properties.local_surface_id.ToString());
  // If the |rect_in_local_root| or current ScreenInfo of the frame has
  // changed, then the viz::LocalSurfaceId must also change.
  if ((last_received_local_frame_size_ != visual_properties.local_frame_size ||
       screen_infos_.current() != visual_properties.screen_infos.current() ||
       capture_sequence_number() != visual_properties.capture_sequence_number ||
       last_received_zoom_level_ != visual_properties.zoom_level ||
       last_received_css_zoom_factor_ != visual_properties.css_zoom_factor) &&
      local_surface_id_ == visual_properties.local_surface_id) {
    bad_message::ReceivedBadMessage(
        frame_proxy_in_parent_renderer_->GetProcess(),
        bad_message::CPFC_RESIZE_PARAMS_CHANGED_LOCAL_SURFACE_ID_UNCHANGED);
    return;
  }

  SynchronizeVisualProperties(visual_properties);
}

void CrossProcessFrameConnector::UpdateViewportIntersection(
    const blink::mojom::ViewportIntersectionState& intersection_state,
    const std::optional<blink::FrameVisualProperties>& visual_properties) {
  bool intersection_changed = !intersection_state.Equals(intersection_state_);
  if (intersection_changed) {
    RenderWidgetHostImpl* host = view_ ? view_->host() : nullptr;
    bool main_frame = host && host->owner_delegate();
    bool visual_properties_changed = false;
    if (visual_properties.has_value()) {
      // Subtlety: RenderWidgetHostViewChildFrame::UpdateViewportIntersection()
      // will quietly fail to propagate the new intersection state for main
      // frames, including fenced frames. For those cases, we need to ensure
      // that the updated VisualProperties are still propagated.
      std::optional<blink::VisualProperties> last_properties;
      if (host && !main_frame)
        last_properties = host->LastComputedVisualProperties();
      SynchronizeVisualProperties(visual_properties.value(), main_frame);
      if (host && !main_frame) {
        visual_properties_changed =
            last_properties != host->LastComputedVisualProperties();
      }
    }
    UpdateViewportIntersectionInternal(intersection_state,
                                       visual_properties_changed);
  } else if (visual_properties.has_value()) {
    SynchronizeVisualProperties(visual_properties.value(), true);
  }
}

void CrossProcessFrameConnector::UpdateViewportIntersectionInternal(
    const blink::mojom::ViewportIntersectionState& intersection_state,
    bool include_visual_properties) {
  intersection_state_ = intersection_state;
  if (view_) {
    CHECK(current_child_frame_host());
    current_child_frame_host()
        ->delegate()
        ->OnRemoteSubframeViewportIntersectionStateChanged(
            current_child_frame_host(), intersection_state);

    // Only ship over the visual properties if they were included in the update
    // viewport intersection message.
    view_->UpdateViewportIntersection(
        intersection_state_, include_visual_properties
                                 ? view_->host()->LastComputedVisualProperties()
                                 : std::nullopt);
  }

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

  // TODO(crbug.com/40103184) Remove this CHECK when the bug is fixed.
  CHECK(current_child_frame_host());
  current_child_frame_host()->VisibilityChanged(visibility_);

  // If there is an inner WebContents, it should be notified of the change in
  // the visibility. The Show/Hide methods will not be called if an inner
  // WebContents exists since the corresponding WebContents will itself call
  // Show/Hide on all the RenderWidgetHostViews (including this) one.
  if (view_->host()
          ->frame_tree()
          ->delegate()
          ->OnRenderFrameProxyVisibilityChanged(frame_proxy_in_parent_renderer_,
                                                visibility_)) {
    return;
  }

  if (visible && !view_->host()->frame_tree()->IsHidden()) {
    view_->Show();
  } else if (!visible) {
    view_->Hide();
  }
}

void CrossProcessFrameConnector::SetIsInert(bool inert) {
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

  RenderFrameHostImpl* root =
      current_child_frame_host()
          ->GetOutermostMainFrameOrEmbedderExcludingProspectiveOwners();
  return static_cast<RenderWidgetHostViewBase*>(root->GetView());
}

RenderWidgetHostViewBase*
CrossProcessFrameConnector::GetParentRenderWidgetHostView() {
  // Input always hits the parent view if there is one so we should
  // escape to an embedder.
  RenderFrameHostImpl* parent =
      current_child_frame_host()
          ? current_child_frame_host()
                ->GetParentOrOuterDocumentOrEmbedderExcludingProspectiveOwners()
          : nullptr;
  return parent ? static_cast<RenderWidgetHostViewBase*>(parent->GetView())
                : nullptr;
}

void CrossProcessFrameConnector::EnableAutoResize(const gfx::Size& min_size,
                                                  const gfx::Size& max_size) {
  frame_proxy_in_parent_renderer_->EnableAutoResize(min_size, max_size);
}

void CrossProcessFrameConnector::DisableAutoResize() {
  frame_proxy_in_parent_renderer_->DisableAutoResize();
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
  frame_proxy_in_parent_renderer_->DidUpdateVisualProperties(metadata);
}

void CrossProcessFrameConnector::SetVisibilityForChildViews(
    bool visible) const {
  current_child_frame_host()->SetVisibilityForChildViews(visible);
}

void CrossProcessFrameConnector::SetLocalFrameSize(
    const gfx::Size& local_frame_size) {
  has_size_ = true;
  const float dsf = screen_infos_.current().device_scale_factor;
  local_frame_size_in_pixels_ = local_frame_size;
  local_frame_size_in_dip_ =
      gfx::ScaleToRoundedSize(local_frame_size, 1.f / dsf);
}

void CrossProcessFrameConnector::SetRectInParentView(
    const gfx::Rect& rect_in_parent_view) {
  gfx::Rect old_rect = rect_in_parent_view_in_dip_;
  const float dsf = screen_infos_.current().device_scale_factor;
  rect_in_parent_view_in_dip_ = gfx::Rect(
      gfx::ScaleToFlooredPoint(rect_in_parent_view.origin(), 1.f / dsf),
      gfx::ScaleToCeiledSize(rect_in_parent_view.size(), 1.f / dsf));

  if (view_ && frame_proxy_in_parent_renderer_) {
    view_->SetBounds(rect_in_parent_view_in_dip_);

    // Other local root frames nested underneath this one implicitly have their
    // view rects changed when their ancestor is repositioned, and therefore
    // need to have their screen rects updated.
    FrameTreeNode* proxy_node =
        frame_proxy_in_parent_renderer_->frame_tree_node();
    if (old_rect.x() != rect_in_parent_view_in_dip_.x() ||
        old_rect.y() != rect_in_parent_view_in_dip_.y()) {
      for (FrameTreeNode* node :
           proxy_node->frame_tree().SubtreeNodes(proxy_node)) {
        if (node != proxy_node && node->current_frame_host()->is_local_root())
          node->current_frame_host()->GetRenderWidgetHost()->SendScreenRects();
      }
    }
  }
}

void CrossProcessFrameConnector::ResetRectInParentView() {
  local_surface_id_ = viz::LocalSurfaceId();
  // TODO(lfg): Why do we need to reset the rect_in_parent_view_in_dip_ that
  // comes from the parent when setting the child? https://crbug.com/809275
  rect_in_parent_view_in_dip_ = gfx::Rect();
  last_received_local_frame_size_ = gfx::Size();
}

void CrossProcessFrameConnector::UpdateRenderThrottlingStatus(
    bool is_throttled,
    bool subtree_throttled,
    bool display_locked) {
  if (is_throttled != is_throttled_ ||
      subtree_throttled != subtree_throttled_ ||
      display_locked != display_locked_) {
    is_throttled_ = is_throttled;
    subtree_throttled_ = subtree_throttled;
    display_locked_ = display_locked;
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

bool CrossProcessFrameConnector::IsDisplayLocked() const {
  return display_locked_;
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

  if (child_frame_crash_shown_closure_for_testing_)
    std::move(child_frame_crash_shown_closure_for_testing_).Run();

  return true;
}

void CrossProcessFrameConnector::MaybeLogShownCrash(
    ShownAfterCrashingReason reason) {
  // Check if an ancestor frame has a pending cross-document navigation.  If
  // so, log the sad frame visibility differently, since the sad frame is
  // expected to go away shortly.  Note that this also handles the common case
  // of a hidden tab with a sad frame being auto-reloaded when it becomes
  // shown.
  bool has_pending_navigation = false;
  for (auto* parent = current_child_frame_host()->GetParentOrOuterDocument();
       parent; parent = parent->GetParentOrOuterDocument()) {
    if (parent->frame_tree_node()->HasPendingCrossDocumentNavigation()) {
      has_pending_navigation = true;
      break;
    }
  }
  auto crash_visibility = has_pending_navigation
                              ? CrashVisibility::kShownWhileAncestorIsLoading
                              : CrashVisibility::kShownAfterCrashing;
  if (!MaybeLogCrash(crash_visibility))
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
  if (visibility_ == blink::mojom::FrameVisibility::kNotRendered ||
      intersection_state().viewport_intersection.IsEmpty()) {
    return false;
  }

  if (!current_child_frame_host()) {
    return true;
  }

  if (EmbedderVisibility() != Visibility::VISIBLE) {
    return false;
  }

  return true;
}

Visibility CrossProcessFrameConnector::EmbedderVisibility() {
  return current_child_frame_host()->delegate()->GetVisibility();
}

RenderFrameHostImpl* CrossProcessFrameConnector::current_child_frame_host()
    const {
  return frame_proxy_in_parent_renderer_
             ? frame_proxy_in_parent_renderer_->frame_tree_node()
                   ->current_frame_host()
             : nullptr;
}

}  // namespace content
