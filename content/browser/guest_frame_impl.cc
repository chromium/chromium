// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/guest_frame_impl.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_local.h"
#include "components/input/cursor_manager.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_manager.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/geometry/dip_util.h"

// TODO(secure-embed) I believe non-public code in /content is not supposed to
// use the public APIs if there are /content implementations. It should just use
// the implementation directly. Review all such includes below.
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/secure_embed_delegate.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

// Nested observer class that forwards relevant events to GuestFrameImpl.
// This avoids function name collisions with CrossProcessFrameConnectorBase.
class GuestFrameImpl::Observer : public WebContentsObserver {
 public:
  explicit Observer(GuestFrameImpl* guest_frame, WebContents* web_contents)
      : WebContentsObserver(web_contents), guest_frame_(guest_frame) {}

  ~Observer() override = default;

  // WebContentsObserver:
  void RenderViewReady() override { guest_frame_->OnRenderViewReady(); }

  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override {
    guest_frame_->OnRenderFrameHostChanged(old_host, new_host);
  }

 private:
  raw_ptr<GuestFrameImpl> guest_frame_;
};

// static
std::unique_ptr<GuestFrame> GuestFrame::Create(WebContents* guest_web_contents,
                                               GuestFrame::Delegate* delegate) {
  return std::make_unique<GuestFrameImpl>(guest_web_contents, delegate);
}

GuestFrameImpl::GuestFrameImpl(WebContents* guest_web_contents,
                               GuestFrame::Delegate* delegate)
    : delegate_(delegate), guest_web_contents_(guest_web_contents) {
  observer_ = std::make_unique<Observer>(this, guest_web_contents);

  // TODO(secure-embed): There may not be a view yet, depending on if the
  // WebContents has been shown or navigated. That means calling GetScreenInfos
  // on the RenderWidgetHost would default to the primary display, which may not
  // be appropriate. So instead calling GetScreenInfos() on the root
  // RenderWidgetHost would provide the right screen information. And,
  // subsequent updates to |screen_infos_| also come from the root. Note that
  // the below call is not necessarily the root though if there are multiple
  // levels of embedding.
  screen_infos_ = current_child_frame_host()
                      ->GetOutermostMainFrameOrEmbedder()
                      ->GetRenderWidgetHost()
                      ->GetScreenInfos();

  UpdateViewForCurrentRenderFrameHost();
}

GuestFrameImpl::~GuestFrameImpl() {
  // Notify the view of this object being destroyed, if the view still exists.
  SetView(nullptr, /*allow_paint_holding=*/false);
}

const viz::FrameSinkId& GuestFrameImpl::GetFrameSinkId() const {
  return frame_sink_id_;
}

void GuestFrameImpl::SetView(RenderWidgetHostViewChildFrame* view,
                             bool allow_paint_holding) {
  // Detach ourselves from the previous |view_|.
  if (view_) {
    RenderWidgetHostViewBase* root_view = GetRootRenderWidgetHostView();
    if (root_view && root_view->GetCursorManager()) {
      root_view->GetCursorManager()->ViewBeingDestroyed(view_);
    }

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
    view_->SetFrameConnector(this);
    if (visibility_ != blink::mojom::FrameVisibility::kRenderedInViewport) {
      OnVisibilityChanged(visibility_);
    }

    frame_sink_id_ = view_->GetFrameSinkId();
    delegate_->SetFrameSinkId(frame_sink_id_);
  }
}

RenderWidgetHostViewBase* GuestFrameImpl::GetParentRenderWidgetHostView() {
  return static_cast<RenderWidgetHostViewBase*>(
      guest_web_contents_->GetSecureEmbedDelegate()
          ->GetEmbedderWebContents()
          ->GetRenderWidgetHostView());
}

RenderWidgetHostViewBase* GuestFrameImpl::GetRootRenderWidgetHostView() {
  // TODO(secure-embed): Do we support multiple levels of embedding?
  // Mixed kinds?
  return GetParentRenderWidgetHostView();
}

void GuestFrameImpl::RenderProcessGone() {
  NOTIMPLEMENTED();
}

void GuestFrameImpl::FirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  NOTIMPLEMENTED();
}

void GuestFrameImpl::SendIntrinsicSizingInfoToParent(
    blink::mojom::IntrinsicSizingInfoPtr) {
  NOTIMPLEMENTED();
}

void GuestFrameImpl::SynchronizeVisualProperties(
    const blink::FrameVisualProperties& visual_properties,
    bool propagate) {
  last_received_zoom_level_ = visual_properties.zoom_level;
  last_received_css_zoom_factor_ = visual_properties.css_zoom_factor;
  last_received_local_frame_size_ = visual_properties.local_frame_size;
  screen_infos_ = visual_properties.screen_infos;
  local_surface_id_ = visual_properties.local_surface_id;

  // TODO(secure-embed): Not implemented yet.
  capture_sequence_number_ = visual_properties.capture_sequence_number;

  SetRectInParentView(visual_properties.rect_in_local_root);
  SetLocalFrameSize(visual_properties.local_frame_size);

  if (!view_) {
    return;
  }

  view_->UpdateScreenInfo();

  RenderWidgetHostImpl* render_widget_host = view_->host();
  DCHECK(render_widget_host);

  // TODO(secure-embed): If we don't support auto-resize, we can skip this call
  // and use something other than FrameVisualProperties.
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

void GuestFrameImpl::UpdateCursor(const ui::Cursor& cursor) {
  RenderWidgetHostViewBase* root_view = GetRootRenderWidgetHostView();
  // UpdateCursor messages are ignored if the root view does not support
  // cursors.
  if (root_view && root_view->GetCursorManager()) {
    root_view->GetCursorManager()->UpdateCursor(view_, cursor);
  }
}

CrossProcessFrameConnectorBase::RootViewFocusState GuestFrameImpl::HasFocus() {
  NOTIMPLEMENTED();
  return CrossProcessFrameConnectorBase::RootViewFocusState::kNullView;
}

void GuestFrameImpl::FocusRootView() {
  NOTIMPLEMENTED();
}

blink::mojom::PointerLockResult GuestFrameImpl::LockPointer(
    bool request_unadjusted_movement) {
  NOTIMPLEMENTED();
  return blink::mojom::PointerLockResult::kUnknownError;
}

blink::mojom::PointerLockResult GuestFrameImpl::ChangePointerLock(
    bool request_unadjusted_movement) {
  NOTIMPLEMENTED();
  return blink::mojom::PointerLockResult::kUnknownError;
}

void GuestFrameImpl::UnlockPointer() {
  NOTIMPLEMENTED();
}

bool GuestFrameImpl::HasSize() const {
  return has_size_;
}

const display::ScreenInfos& GuestFrameImpl::GetScreenInfos() const {
  return screen_infos_;
}

const viz::LocalSurfaceId& GuestFrameImpl::GetLocalSurfaceId() const {
  return local_surface_id_;
}

const blink::mojom::ViewportIntersectionState&
GuestFrameImpl::GetIntersectionState() const {
  return intersection_state_;
}

uint32_t GuestFrameImpl::GetCaptureSequenceNumber() const {
  NOTIMPLEMENTED();
  return 0;
}

const gfx::Rect& GuestFrameImpl::GetRectInParentViewInDip() const {
  return rect_in_parent_view_in_dip_;
}

const gfx::Size& GuestFrameImpl::GetLocalFrameSizeInDip() const {
  return local_frame_size_in_dip_;
}

const gfx::Size& GuestFrameImpl::GetLocalFrameSizeInPixels() const {
  return local_frame_size_in_pixels_;
}

double GuestFrameImpl::GetCssZoomFactor() const {
  return last_received_css_zoom_factor_;
}

void GuestFrameImpl::EnableAutoResize(const gfx::Size& min_size,
                                      const gfx::Size& max_size) {
  NOTIMPLEMENTED();
}

void GuestFrameImpl::DisableAutoResize() {
  NOTIMPLEMENTED();
}

bool GuestFrameImpl::IsInert() const {
  return is_inert_;
}

cc::TouchAction GuestFrameImpl::InheritedEffectiveTouchAction() const {
  return inherited_effective_touch_action_;
}

bool GuestFrameImpl::IsHidden() const {
  return visibility_ == blink::mojom::FrameVisibility::kNotRendered;
}

bool GuestFrameImpl::IsThrottled() const {
  return is_throttled_;
}

bool GuestFrameImpl::IsSubtreeThrottled() const {
  return subtree_throttled_;
}

bool GuestFrameImpl::IsDisplayLocked() const {
  return display_locked_;
}

void GuestFrameImpl::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {
  NOTIMPLEMENTED();
  // TODO(secure-embed): Need to pass the visual properties update to the
  // embedder. When this is done, need to make sure that the two endpoints don't
  // get into an infinite cycle of updates.
  // frame_proxy_in_parent_renderer_->DidUpdateVisualProperties(metadata);
}

void GuestFrameImpl::SetVisibilityForChildViews(bool visible) const {
  current_child_frame_host()->SetVisibilityForChildViews(visible);
}

void GuestFrameImpl::SetLocalFrameSize(const gfx::Size& local_frame_size) {
  has_size_ = true;
  const float dsf = screen_infos_.current().device_scale_factor;
  local_frame_size_in_pixels_ = local_frame_size;
  local_frame_size_in_dip_ =
      gfx::ScaleToRoundedSize(local_frame_size, 1.f / dsf);
}

void GuestFrameImpl::SetRectInParentView(const gfx::Rect& rect_in_parent_view) {
  const float dsf = screen_infos_.current().device_scale_factor;
  rect_in_parent_view_in_dip_ = gfx::Rect(
      gfx::ScaleToFlooredPoint(rect_in_parent_view.origin(), 1.f / dsf),
      gfx::ScaleToCeiledSize(rect_in_parent_view.size(), 1.f / dsf));

  // TODO(secure-embed): The CPFC version of this checks if
  // frame_proxy_in_parent_renderer_ is valid before proceeding. It looks like
  // this is because it then iterates through the FrameTreeNodes of the subtree
  // rooted at the frame proxy to update their screen rects. Since we don't have
  // frame proxies, what do we do here instead?
  if (view_ /*&& frame_proxy_in_parent_renderer_*/) {
    view_->SetBounds(rect_in_parent_view_in_dip_);

    // TODO(secure-embed): Notify the embedder of the rect change so that it can
    // call SendScreenRects on all subtrees rooted at the guest web contents?

    // Other local root frames nested underneath this one implicitly have their
    // view rects changed when their ancestor is repositioned, and therefore
    // need to have their screen rects updated.
    // FrameTreeNode* proxy_node =
    //    frame_proxy_in_parent_renderer_->frame_tree_node();
    // if (old_rect.x() != rect_in_parent_view_in_dip_.x() ||
    //    old_rect.y() != rect_in_parent_view_in_dip_.y()) {
    //  for (FrameTreeNode* node :
    //       proxy_node->frame_tree().SubtreeNodes(proxy_node)) {
    //    if (node != proxy_node && node->current_frame_host()->is_local_root())
    //      node->current_frame_host()->GetRenderWidgetHost()->SendScreenRects();
    //  }
    //}
  }
}

void GuestFrameImpl::SetIsInert(bool inert) {
  // TODO(secure-embed): Do we want to support inert and other throttling states
  // across embedder/guest boundaries?
  is_inert_ = inert;
  if (view_) {
    view_->SetIsInert();
  }
}

void GuestFrameImpl::OnSetInheritedEffectiveTouchAction(
    cc::TouchAction touch_action) {
  // TODO(secure-embed): Do we want to support inheriting touch actions?
  inherited_effective_touch_action_ = touch_action;
  if (view_) {
    view_->UpdateInheritedEffectiveTouchAction();
  }
}

void GuestFrameImpl::OnVisibilityChanged(
    blink::mojom::FrameVisibility visibility) {
  bool visible = visibility != blink::mojom::FrameVisibility::kNotRendered;
  visibility_ = visibility;

  if (!view_) {
    return;
  }

  current_child_frame_host()->VisibilityChanged(visibility_);

  // TODO(secure-embed): Needs finished.

  // If there is an inner WebContents, it should be notified of the change in
  // the visibility. The Show/Hide methods will not be called if an inner
  // WebContents exists since the corresponding WebContents will itself call
  // Show/Hide on all the RenderWidgetHostViews (including this) one.
  // if (view_->host()
  //        ->frame_tree()
  //        ->delegate()
  //        ->OnRenderFrameProxyVisibilityChanged(frame_proxy_in_parent_renderer_,
  //                                              visibility_)) {
  //  return;
  //}

  if (visible && !view_->host()->frame_tree()->IsHidden()) {
    view_->Show();
  } else if (!visible) {
    view_->Hide();
  }
}

void GuestFrameImpl::UpdateRenderThrottlingStatus(bool is_throttled,
                                                  bool subtree_throttled,
                                                  bool display_locked) {
  if (is_throttled != is_throttled_ ||
      subtree_throttled != subtree_throttled_ ||
      display_locked != display_locked_) {
    is_throttled_ = is_throttled;
    subtree_throttled_ = subtree_throttled;
    display_locked_ = display_locked;
    if (view_) {
      view_->UpdateRenderThrottlingStatus();
    }
  }
}

void GuestFrameImpl::UpdateViewportIntersection(
    const blink::mojom::ViewportIntersectionState& intersection_state,
    const std::optional<blink::FrameVisualProperties>& visual_properties) {
  // TODO(secure-embed): Implementation was copied from CPFC but need to dig
  // into it and better understand if, and how much, of it is needed.
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
      if (host && !main_frame) {
        last_properties = host->LastComputedVisualProperties();
      }
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

bool GuestFrameImpl::IsVisible() {
  if (visibility_ == blink::mojom::FrameVisibility::kNotRendered ||
      GetIntersectionState().viewport_intersection.IsEmpty()) {
    return false;
  }

  if (EmbedderVisibility() != Visibility::VISIBLE) {
    return false;
  }

  return true;
}

Visibility GuestFrameImpl::EmbedderVisibility() {
  // TODO(secure-embed): Need to get the embedder visibility rather than the
  // embedded visibility. Embedder = SecureEmbed. May need to add a method to
  // SecureEmbedDelegate for this or ensure that visibility state is pushed to
  // the GuestFrame when it changes.
  NOTIMPLEMENTED();
  return guest_web_contents_->GetSecureEmbedDelegate()
      ->GetEmbedderWebContents()
      ->GetVisibility();
}

void GuestFrameImpl::OnSynchronizeVisualProperties(
    const blink::FrameVisualProperties& visual_properties) {
  // If the |rect_in_local_root| or current ScreenInfo of the frame has
  // changed, then the viz::LocalSurfaceId must also change.
  if ((last_received_local_frame_size_ != visual_properties.local_frame_size ||
       screen_infos_.current() != visual_properties.screen_infos.current() ||
       GetCaptureSequenceNumber() !=
           visual_properties.capture_sequence_number ||
       last_received_zoom_level_ != visual_properties.zoom_level ||
       last_received_css_zoom_factor_ != visual_properties.css_zoom_factor) &&
      local_surface_id_ == visual_properties.local_surface_id) {
    bad_message::ReceivedBadMessage(
        guest_web_contents_->GetSecureEmbedDelegate()
            ->GetEmbedderWebContents()
            ->GetPrimaryMainFrame()
            ->GetProcess(),
        bad_message::CPFC_RESIZE_PARAMS_CHANGED_LOCAL_SURFACE_ID_UNCHANGED);
    return;
  }

  SynchronizeVisualProperties(visual_properties);
}

input::RenderWidgetHostViewInput* GuestFrameImpl::GetParentViewInput() {
  NOTIMPLEMENTED();
  return nullptr;
}

input::RenderWidgetHostViewInput* GuestFrameImpl::GetRootViewInput() {
  NOTIMPLEMENTED();
  return nullptr;
}

void GuestFrameImpl::OnRenderViewReady() {
  // When the RenderView is ready, update the view in case it has changed.
  UpdateViewForCurrentRenderFrameHost();
}

void GuestFrameImpl::OnRenderFrameHostChanged(RenderFrameHost* old_host,
                                              RenderFrameHost* new_host) {
  // When the RenderFrameHost changes, we need to update the view to track
  // the new RenderWidgetHostView associated with the new RenderFrameHost.
  UpdateViewForCurrentRenderFrameHost();
}

void GuestFrameImpl::UpdateViewForCurrentRenderFrameHost() {
  // Get the current RenderWidgetHostView for the guest WebContents.
  auto* base_view = static_cast<RenderWidgetHostViewBase*>(
      guest_web_contents_->GetRenderWidgetHostView());

  if (!base_view) {
    SetView(nullptr, /*allow_paint_holding=*/false);
    return;
  }

  if (!base_view->IsRenderWidgetHostViewChildFrame()) {
    return;
  }

  auto* child_view = static_cast<RenderWidgetHostViewChildFrame*>(base_view);

  if (view_ != child_view) {
    SetView(child_view, /*allow_paint_holding=*/false);
  }
}

void GuestFrameImpl::ResetRectInParentView() {
  local_surface_id_ = viz::LocalSurfaceId();
  rect_in_parent_view_in_dip_ = gfx::Rect();
  last_received_local_frame_size_ = gfx::Size();
}

void GuestFrameImpl::DelegateWasShown() {
  NOTIMPLEMENTED();
}

void GuestFrameImpl::UpdateViewportIntersectionInternal(
    const blink::mojom::ViewportIntersectionState& intersection_state,
    bool include_visual_properties) {
  intersection_state_ = intersection_state;
  if (view_) {
    // TODO(secure-embed): Look into this further to make sure we're calling it
    // on the right side (embedder vs embedded).
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
}

RenderFrameHostImpl* GuestFrameImpl::current_child_frame_host() const {
  return static_cast<WebContentsImpl*>(guest_web_contents_)
      ->GetPrimaryMainFrame();
}

}  // namespace content
