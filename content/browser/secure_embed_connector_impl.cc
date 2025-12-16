// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/secure_embed_connector_impl.h"

#include "base/notimplemented.h"
#include "components/input/cursor_manager.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/frame_tree.h"
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
#include "content/public/browser/web_contents_observer.h"

namespace content {

// Nested observer class that forwards relevant events to
// SecureEmbedConnectorImpl. This avoids function name collisions with
// CrossProcessFrameConnectorBase.
class SecureEmbedConnectorImpl::WCObserver : public WebContentsObserver {
 public:
  explicit WCObserver(SecureEmbedConnectorImpl* guest_frame,
                      WebContents* web_contents)
      : WebContentsObserver(web_contents), guest_frame_(guest_frame) {}

  ~WCObserver() override = default;

  // WebContentsObserver:
  void RenderViewReady() override { guest_frame_->OnRenderViewReady(); }

 private:
  raw_ptr<SecureEmbedConnectorImpl> guest_frame_;
};

// static
void SecureEmbedConnector::Attach(WebContents* parent_web_contents,
                                  WebContents* child_web_contents,
                                  SecureEmbedConnector::Delegate* delegate) {
  // Must Detach the child before re-Attaching.
  CHECK(!child_web_contents->GetSecureEmbedConnector());
  auto connector = std::make_unique<SecureEmbedConnectorImpl>(
      static_cast<WebContentsImpl*>(parent_web_contents),
      static_cast<WebContentsImpl*>(child_web_contents), delegate);
  auto* connector_ptr = connector.get();
  static_cast<WebContentsImpl*>(child_web_contents)
      ->SetSecureEmbedConnector(std::move(connector));
  connector_ptr->UpdateViewForCurrentRenderFrameHost();
}

// static
void SecureEmbedConnector::Detach(WebContents* child_web_contents) {
  if (auto* connector = static_cast<SecureEmbedConnectorImpl*>(
          child_web_contents->GetSecureEmbedConnector())) {
    // Note: we detach delegate after changing visibility so that
    // performance_manager doesn't get perturbed by us messing w/visibility of
    // something not top-level.
    connector->OnVisibilityChanged(blink::mojom::FrameVisibility::kNotRendered);
  }

  // Connector will be freed by ClearSecureEmbedConnector().
  static_cast<WebContentsImpl*>(child_web_contents)
      ->ClearSecureEmbedConnector();
}

SecureEmbedConnectorImpl::SecureEmbedConnectorImpl(
    WebContentsImpl* embedder_web_contents,
    WebContentsImpl* embedded_web_contents,
    SecureEmbedConnector::Delegate* delegate)
    : delegate_(delegate),
      embedder_web_contents_(embedder_web_contents->GetWeakPtr()),
      guest_web_contents_(embedded_web_contents) {
  observer_ = std::make_unique<WCObserver>(this, embedded_web_contents);

  // TODO(secure-embed): There may not be a view yet, depending on if the
  // WebContents has been shown or navigated. That means calling GetScreenInfos
  // on the RenderWidgetHost would default to the primary display, which may not
  // be appropriate. So instead calling GetScreenInfos() on the root
  // RenderWidgetHost would provide the right screen information. And,
  // subsequent updates to |screen_infos_| also come from the root. Note that
  // the below call is not necessarily the root though if there are multiple
  // levels of embedding.
  screen_infos_ = embedder_web_contents->GetPrimaryMainFrame()
                      ->GetOutermostMainFrameOrEmbedder()
                      ->GetRenderWidgetHost()
                      ->GetScreenInfos();
}

SecureEmbedConnectorImpl::~SecureEmbedConnectorImpl() {
  // Notify the view of this object being destroyed, if the view still exists.
  SetView(nullptr, /*allow_paint_holding=*/false);
}

WebContentsView* SecureEmbedConnectorImpl::GetEmbedderWebContentsView() {
  if (embedder_web_contents()) {
    return embedder_web_contents()->GetView();
  }
  return nullptr;
}

RenderViewHostDelegateView*
SecureEmbedConnectorImpl::GetEmbedderRenderViewHostDelegateView() {
  if (embedder_web_contents()) {
    return embedder_web_contents()->GetDelegateView();
  }
  return nullptr;
}

void SecureEmbedConnectorImpl::EmbedderSystemDragEnded(
    RenderWidgetHost* source_rwh) {
  if (embedder_web_contents()) {
    embedder_web_contents()->SystemDragEnded(source_rwh);
  }
}

input::RenderWidgetHostInputEventRouter*
SecureEmbedConnectorImpl::GetInputEventRouter() {
  return embedder_web_contents()->GetInputEventRouter();
}

TextInputManager* SecureEmbedConnectorImpl::GetTextInputManager() {
  return embedder_web_contents()->GetTextInputManager();
}

void SecureEmbedConnectorImpl::FocusInEmbedder(FocusOperation focus_op) {
  // Focus the embedder when traversing out of <embed>.
  // Usually when an *unfocused* Blink frame gains focus, it calls
  // RFHI::DidFocusFrame() which makes the WebContents focused.
  // However in secure embed, when the guest WebContents is focused, its
  // embedder Blink frame is also focused, as a result Blink does *not* call
  // DidFocusFrame() when the focus traverses out. Hence focusing the embedder
  // WebContents explicitly here.
  if (focus_op == FocusOperation::kFocusBeforePlugin ||
      focus_op == FocusOperation::kFocusAfterPlugin) {
    embedder_web_contents()->SetAsFocusedWebContentsIfNecessary();
  }
  if (delegate_) {
    delegate_->FocusInEmbedder(focus_op);
  }
}

// static
bool SecureEmbedConnectorImpl::ContainsOrIsFocusedWebContents(
    WebContentsImpl* web_contents) {
  WebContentsImpl* root_web_contents = GetRootWebContents(web_contents);
  WebContentsImpl* focused_web_contents =
      root_web_contents->GetFocusedWebContents();
  while (focused_web_contents) {
    if (focused_web_contents == web_contents) {
      return true;
    }
    focused_web_contents = GetParentWebContents(focused_web_contents);
  }

  return false;
}

FrameTree* SecureEmbedConnectorImpl::GetFocusedFrameTree() {
  return ContainsOrIsFocusedWebContents(guest_web_contents_)
             ? embedder_web_contents()->GetFocusedFrameTree()
             : nullptr;
}

void SecureEmbedConnectorImpl::SetFocusedFrameTree(
    FrameTree* frame_tree_to_focus) {
  if (!embedder_web_contents_) {
    return;
  }

  // Update focused frame tree stored in the embedder.
  embedder_web_contents()->SetFocusedFrameTree(frame_tree_to_focus);
  // The `frame_tree_to_focus` must belong to this WebContents
  // or an inner WebContents in the subtree.
  CHECK(ContainsOrIsFocusedWebContents(guest_web_contents_));

  // Ensure that outer frame trees are focused.
  embedder_web_contents()->GetPrimaryFrameTree().FocusOuterFrameTrees();

  // Ensure that the embedder's page has focus so that it can display active UI
  // and therefore the embedded plugin is also active.
  embedder_web_contents()
      ->GetPrimaryMainFrame()
      ->GetRenderWidgetHost()
      ->SetPageFocus(true);
}

void SecureEmbedConnectorImpl::ClearFocusOnInnerWebContents() {
  if (!ContainsOrIsFocusedWebContents(guest_web_contents_)) {
    return;
  }
  CHECK(embedder_web_contents_)
      << "focused frame tree should be cleared before detachment";

  // Using the same logic as the one for inner WebContents in WebContentsImpl
  // destructor.
  embedder_web_contents()
      ->GetOutermostWebContents()
      ->SetAsFocusedWebContentsIfNecessary();
}

SecureEmbedConnector::Delegate* SecureEmbedConnectorImpl::GetDelegate() {
  return delegate_;
}

void SecureEmbedConnectorImpl::SetFocus(bool focused,
                                        blink::mojom::FocusType focus_type) {
  if (!guest_web_contents_ || !view_) {
    return;
  }

  view_->host()->SetPageFocus(focused);

  if (!focused) {
    return;
  }

  if (focus_type == blink::mojom::FocusType::kForward ||
      focus_type == blink::mojom::FocusType::kBackward) {
    static_cast<RenderViewHostImpl*>(guest_web_contents_->GetRenderViewHost())
        ->SetInitialFocus(
            /*reverse=*/focus_type == blink::mojom::FocusType::kBackward);
  }

  // Ensure that the embedded frame tree is the focused frame tree if it is
  // not already the focused frame tree when the plugin becomes in focus.
  // Skip the check for kPage. kPage doesn't involved focused frame change. It
  // happens when OS window get/lost focus or for parent pages when child page
  // is in focus, as part of FocusOuterFrameTrees().
  if ((focus_type != blink::mojom::FocusType::kPage) &&
      !ContainsOrIsFocusedWebContents(guest_web_contents_)) {
    SetFocusedFrameTree(&guest_web_contents_->GetPrimaryFrameTree());
  }
}

const viz::FrameSinkId& SecureEmbedConnectorImpl::GetFrameSinkId() const {
  return frame_sink_id_;
}

void SecureEmbedConnectorImpl::SetView(RenderWidgetHostViewChildFrame* view,
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
    if (delegate_) {
      delegate_->SetFrameSinkId(frame_sink_id_);
    }
  }
}

RenderWidgetHostViewBase*
SecureEmbedConnectorImpl::GetParentRenderWidgetHostView() {
  if (!embedder_web_contents_) {
    return nullptr;
  }
  return static_cast<RenderWidgetHostViewBase*>(
      embedder_web_contents()->GetRenderWidgetHostView());
}

RenderWidgetHostViewBase*
SecureEmbedConnectorImpl::GetRootRenderWidgetHostView() {
  // TODO(secure-embed): Do we support multiple levels of embedding?
  // Mixed kinds?
  return GetParentRenderWidgetHostView();
}

void SecureEmbedConnectorImpl::RenderProcessGone() {
  if (delegate_) {
    delegate_->ChildProcessGone();
  }

  // TODO(secure-embed): CrossProcessFrameConnector does a lot of logging
  // and sometimes reloading here that's about child frames. We need to decide
  // when that's even relevant here (it's not for the very basic Webium use),
  // and replicate it.
}

void SecureEmbedConnectorImpl::FirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  NOTIMPLEMENTED();
}

void SecureEmbedConnectorImpl::SendIntrinsicSizingInfoToParent(
    blink::mojom::IntrinsicSizingInfoPtr) {
  NOTIMPLEMENTED();
}

void SecureEmbedConnectorImpl::SynchronizeVisualProperties(
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

void SecureEmbedConnectorImpl::UpdateCursor(const ui::Cursor& cursor) {
  RenderWidgetHostViewBase* root_view = GetRootRenderWidgetHostView();
  // UpdateCursor messages are ignored if the root view does not support
  // cursors.
  if (root_view && root_view->GetCursorManager()) {
    root_view->GetCursorManager()->UpdateCursor(view_, cursor);
  }
}

CrossProcessFrameConnectorBase::RootViewFocusState
SecureEmbedConnectorImpl::HasFocus() {
  NOTIMPLEMENTED();
  return CrossProcessFrameConnectorBase::RootViewFocusState::kNullView;
}

void SecureEmbedConnectorImpl::FocusRootView() {
  NOTIMPLEMENTED();
}

blink::mojom::PointerLockResult SecureEmbedConnectorImpl::LockPointer(
    bool request_unadjusted_movement) {
  NOTIMPLEMENTED();
  return blink::mojom::PointerLockResult::kUnknownError;
}

blink::mojom::PointerLockResult SecureEmbedConnectorImpl::ChangePointerLock(
    bool request_unadjusted_movement) {
  NOTIMPLEMENTED();
  return blink::mojom::PointerLockResult::kUnknownError;
}

void SecureEmbedConnectorImpl::UnlockPointer() {
  NOTIMPLEMENTED();
}

bool SecureEmbedConnectorImpl::HasSize() const {
  return has_size_;
}

const display::ScreenInfos& SecureEmbedConnectorImpl::GetScreenInfos() const {
  return screen_infos_;
}

const viz::LocalSurfaceId& SecureEmbedConnectorImpl::GetLocalSurfaceId() const {
  return local_surface_id_;
}

const blink::mojom::ViewportIntersectionState&
SecureEmbedConnectorImpl::GetIntersectionState() const {
  return intersection_state_;
}

uint32_t SecureEmbedConnectorImpl::GetCaptureSequenceNumber() const {
  NOTIMPLEMENTED();
  return 0;
}

const gfx::Rect& SecureEmbedConnectorImpl::GetRectInParentViewInDip() const {
  return rect_in_parent_view_in_dip_;
}

const gfx::Size& SecureEmbedConnectorImpl::GetLocalFrameSizeInDip() const {
  return local_frame_size_in_dip_;
}

const gfx::Size& SecureEmbedConnectorImpl::GetLocalFrameSizeInPixels() const {
  return local_frame_size_in_pixels_;
}

double SecureEmbedConnectorImpl::GetCssZoomFactor() const {
  return last_received_css_zoom_factor_;
}

void SecureEmbedConnectorImpl::EnableAutoResize(const gfx::Size& min_size,
                                                const gfx::Size& max_size) {
  NOTIMPLEMENTED();
}

void SecureEmbedConnectorImpl::DisableAutoResize() {
  NOTIMPLEMENTED();
}

bool SecureEmbedConnectorImpl::IsInert() const {
  return is_inert_;
}

cc::TouchAction SecureEmbedConnectorImpl::InheritedEffectiveTouchAction()
    const {
  return inherited_effective_touch_action_;
}

bool SecureEmbedConnectorImpl::IsHidden() const {
  return visibility_ == blink::mojom::FrameVisibility::kNotRendered;
}

bool SecureEmbedConnectorImpl::IsThrottled() const {
  return is_throttled_;
}

bool SecureEmbedConnectorImpl::IsSubtreeThrottled() const {
  return subtree_throttled_;
}

bool SecureEmbedConnectorImpl::IsDisplayLocked() const {
  return display_locked_;
}

void SecureEmbedConnectorImpl::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {
  if (metadata.local_surface_id.has_value() &&
      local_surface_id_ != *metadata.local_surface_id) {
    // `local_surface_id_` will be updated in SynchronizeVisualProperties()
    // after a round-trip to the Plugin.
    delegate_->UpdateLocalSurfaceIdFromChild(*metadata.local_surface_id);
  }
}

void SecureEmbedConnectorImpl::SetVisibilityForChildViews(bool visible) const {
  current_child_frame_host()->SetVisibilityForChildViews(visible);
}

void SecureEmbedConnectorImpl::SetLocalFrameSize(
    const gfx::Size& local_frame_size) {
  has_size_ = true;
  const float dsf = screen_infos_.current().device_scale_factor;
  local_frame_size_in_pixels_ = local_frame_size;
  local_frame_size_in_dip_ =
      gfx::ScaleToRoundedSize(local_frame_size, 1.f / dsf);
}

void SecureEmbedConnectorImpl::SetRectInParentView(
    const gfx::Rect& rect_in_parent_view) {
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

void SecureEmbedConnectorImpl::SetIsInert(bool inert) {
  // TODO(secure-embed): Do we want to support inert and other throttling states
  // across embedder/guest boundaries?
  is_inert_ = inert;
  if (view_) {
    view_->SetIsInert();
  }
}

void SecureEmbedConnectorImpl::OnSetInheritedEffectiveTouchAction(
    cc::TouchAction touch_action) {
  // TODO(secure-embed): Do we want to support inheriting touch actions?
  inherited_effective_touch_action_ = touch_action;
  if (view_) {
    view_->UpdateInheritedEffectiveTouchAction();
  }
}

void SecureEmbedConnectorImpl::OnVisibilityChanged(
    blink::mojom::FrameVisibility visibility) {
  bool visible = visibility != blink::mojom::FrameVisibility::kNotRendered;
  visibility_ = visibility;

  if (!view_) {
    return;
  }

  current_child_frame_host()->VisibilityChanged(visibility_);

  switch (visibility) {
    case blink::mojom::FrameVisibility::kRenderedInViewport:
      guest_web_contents_->WasShown();
      break;
    case blink::mojom::FrameVisibility::kNotRendered:
      guest_web_contents_->WasHidden();
      break;
    case blink::mojom::FrameVisibility::kRenderedOutOfViewport:
      guest_web_contents_->WasOccluded();
      break;
  }

  if (visible && !view_->host()->frame_tree()->IsHidden()) {
    view_->Show();
  } else if (!visible) {
    view_->Hide();
  }
}

void SecureEmbedConnectorImpl::UpdateRenderThrottlingStatus(
    bool is_throttled,
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

void SecureEmbedConnectorImpl::UpdateViewportIntersection(
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

bool SecureEmbedConnectorImpl::IsVisible() {
  if (visibility_ == blink::mojom::FrameVisibility::kNotRendered ||
      GetIntersectionState().viewport_intersection.IsEmpty()) {
    return false;
  }

  if (EmbedderVisibility() != Visibility::VISIBLE) {
    return false;
  }

  return true;
}

Visibility SecureEmbedConnectorImpl::EmbedderVisibility() {
  if (!embedder_web_contents()) {
    return Visibility::HIDDEN;
  }

  // TODO(secure-embed): Need to get the embedder visibility rather than the
  // embedded visibility. Embedder = SecureEmbed. May need to add a method to
  // SecureEmbedDelegate for this or ensure that visibility state is pushed to
  // the GuestFrame when it changes.
  NOTIMPLEMENTED();
  return embedder_web_contents()->GetVisibility();
}

void SecureEmbedConnectorImpl::OnSynchronizeVisualProperties(
    const blink::FrameVisualProperties& visual_properties) {
  if (!embedder_web_contents_) {
    return;
  }

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
        embedder_web_contents()->GetPrimaryMainFrame()->GetProcess(),
        bad_message::CPFC_RESIZE_PARAMS_CHANGED_LOCAL_SURFACE_ID_UNCHANGED);
    return;
  }

  SynchronizeVisualProperties(visual_properties);
}

input::RenderWidgetHostViewInput*
SecureEmbedConnectorImpl::GetParentViewInput() {
  return GetParentRenderWidgetHostView();
}

input::RenderWidgetHostViewInput* SecureEmbedConnectorImpl::GetRootViewInput() {
  return GetRootRenderWidgetHostView();
}

// Although SetView is called from the WebContentsImpl during navigation,
// we still need the Observer for RenderViewReady to catch the initial
// creation or the view won't be set correctly for the initial document.
void SecureEmbedConnectorImpl::OnRenderViewReady() {
  // When the RenderView is ready, update the view in case it has changed.
  UpdateViewForCurrentRenderFrameHost();
}

void SecureEmbedConnectorImpl::UpdateViewForCurrentRenderFrameHost() {
  CHECK(guest_web_contents_);  // Should not get here w/o WebContents.

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

// static
WebContentsImpl* SecureEmbedConnectorImpl::GetParentWebContents(
    WebContentsImpl* web_contents) {
  if (SecureEmbedConnector* connector =
          web_contents->GetSecureEmbedConnector()) {
    return static_cast<SecureEmbedConnectorImpl*>(connector)
        ->embedder_web_contents();
  }
  return static_cast<WebContentsImpl*>(web_contents->GetOuterWebContents());
}

// static
WebContentsImpl* SecureEmbedConnectorImpl::GetRootWebContents(
    WebContentsImpl* web_contents) {
  while (GetParentWebContents(web_contents)) {
    web_contents = GetParentWebContents(web_contents);
  }
  return web_contents;
}

WebContentsImpl* SecureEmbedConnectorImpl::embedder_web_contents() {
  return static_cast<WebContentsImpl*>(embedder_web_contents_.get());
}

void SecureEmbedConnectorImpl::ResetRectInParentView() {
  local_surface_id_ = viz::LocalSurfaceId();
  rect_in_parent_view_in_dip_ = gfx::Rect();
  last_received_local_frame_size_ = gfx::Size();
}

void SecureEmbedConnectorImpl::DelegateWasShown() {
  NOTIMPLEMENTED();
}

void SecureEmbedConnectorImpl::UpdateViewportIntersectionInternal(
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

RenderFrameHostImpl* SecureEmbedConnectorImpl::current_child_frame_host()
    const {
  if (!guest_web_contents_) {
    return nullptr;
  }
  return static_cast<WebContentsImpl*>(guest_web_contents_.get())
      ->GetPrimaryMainFrame();
}

}  // namespace content
