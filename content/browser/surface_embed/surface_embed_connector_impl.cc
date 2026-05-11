// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/surface_embed/surface_embed_connector_impl.h"

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
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-shared.h"
#include "third_party/blink/public/mojom/input/pointer_lock_result.mojom.h"
#include "ui/base/cursor/cursor.h"
#include "ui/compositor/compositor.h"

namespace content {

// Forwards notifications about the child web contents to the connector.
// TODO(crbug.com/493315755): Check whether we still need WCObserver now that we
// call UpdateViewForCurrentRenderFrameHost() during Attach().
class SurfaceEmbedConnectorImpl::WCObserver : public WebContentsObserver {
 public:
  explicit WCObserver(SurfaceEmbedConnectorImpl* surface_embed_connector,
                      WebContents* child_web_contents)
      : WebContentsObserver(child_web_contents),
        surface_embed_connector_(surface_embed_connector) {}

  ~WCObserver() override = default;

  // WebContentsObserver:
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override {
    if (render_frame_host->IsInPrimaryMainFrame()) {
      surface_embed_connector_->OnRenderFrameCreated();
    }
  }

 private:
  raw_ptr<SurfaceEmbedConnectorImpl> surface_embed_connector_;
};

// static
void SurfaceEmbedConnector::Attach(WebContents* child_web_contents,
                                   RenderFrameHost* outer_document_rfh,
                                   SurfaceEmbedConnector::Delegate* delegate) {
  CHECK(child_web_contents);
  CHECK(outer_document_rfh);
  WebContents* parent_web_contents =
      WebContents::FromRenderFrameHost(outer_document_rfh);
  CHECK(parent_web_contents);
  // Must Detach the child before re-Attaching.
  CHECK(!child_web_contents->GetSurfaceEmbedConnector());
  auto connector = base::WrapUnique(new SurfaceEmbedConnectorImpl(
      child_web_contents, parent_web_contents, delegate));
  static_cast<WebContentsImpl*>(child_web_contents)
      ->SetSurfaceEmbedConnector(std::move(connector));

  static_cast<WebContentsImpl*>(parent_web_contents)
      ->SurfaceEmbedChildWebContentsAttached(child_web_contents,
                                             outer_document_rfh);
}

// static
void SurfaceEmbedConnector::Detach(WebContents* child_web_contents) {
  if (auto* connector = static_cast<SurfaceEmbedConnectorImpl*>(
          child_web_contents->GetSurfaceEmbedConnector())) {
    // Note: we set visibility to not-rendered prior to detachment because if
    // the WebContents isn't attached to any surface, it won't be rendered so it
    // SHOULD have the visibility of kNotRendered, to prevent
    // visibility/intersection notifications from being sent to it.
    connector->OnVisibilityChanged(blink::mojom::FrameVisibility::kNotRendered);

    if (WebContentsImpl* parent_web_contents =
            connector->parent_web_contents()) {
      parent_web_contents->SurfaceEmbedChildWebContentsDetached(
          child_web_contents);
    }
  }

  // Connector will be freed by ClearSurfaceEmbedConnector().
  static_cast<WebContentsImpl*>(child_web_contents)
      ->ClearSurfaceEmbedConnector();
}

SurfaceEmbedConnectorImpl::SurfaceEmbedConnectorImpl(
    WebContents* child_web_contents,
    WebContents* parent_web_contents,
    SurfaceEmbedConnector::Delegate* delegate)
    : delegate_(delegate),
      child_web_contents_(static_cast<WebContentsImpl*>(child_web_contents)),
      parent_web_contents_(parent_web_contents->GetWeakPtr()) {
  wc_observer_ = std::make_unique<WCObserver>(this, child_web_contents);
  CHECK(current_child_frame_host());

  // Current_child_frame_host must be the primary main frame of the child
  // WebContents.
  CHECK_EQ(current_child_frame_host()->GetOutermostMainFrameOrEmbedder(),
           current_child_frame_host());
  screen_infos_ =
      current_child_frame_host()->GetRenderWidgetHost()->GetScreenInfos();
}

SurfaceEmbedConnectorImpl::~SurfaceEmbedConnectorImpl() {
  SetView(nullptr, /*allow_paint_holding=*/false);
}

WebContentsView* SurfaceEmbedConnectorImpl::GetParentWebContentsView() const {
  return parent_web_contents() ? parent_web_contents()->GetView() : nullptr;
}

RenderViewHostDelegateView*
SurfaceEmbedConnectorImpl::GetParentRenderViewHostDelegateView() const {
  return parent_web_contents() ? parent_web_contents()->GetDelegateView()
                               : nullptr;
}

input::RenderWidgetHostInputEventRouter*
SurfaceEmbedConnectorImpl::GetInputEventRouter() {
  return parent_web_contents() ? parent_web_contents()->GetInputEventRouter()
                               : nullptr;
}

TextInputManager* SurfaceEmbedConnectorImpl::GetTextInputManager() {
  return parent_web_contents() ? parent_web_contents()->GetTextInputManager()
                               : nullptr;
}

SurfaceEmbedConnector::Delegate* SurfaceEmbedConnectorImpl::GetDelegate() {
  return delegate_;
}

const viz::FrameSinkId& SurfaceEmbedConnectorImpl::GetFrameSinkId() const {
  return frame_sink_id_;
}

void SurfaceEmbedConnectorImpl::OnSynchronizeVisualProperties(
    const blink::FrameVisualProperties& visual_properties) {
  // If the `rect_in_local_root` or current ScreenInfo of the frame has
  // changed, then the viz::LocalSurfaceId must also change.
  if ((last_received_local_frame_size_ != visual_properties.local_frame_size ||
       screen_infos_.current() != visual_properties.screen_infos.current() ||
       GetCaptureSequenceNumber() !=
           visual_properties.capture_sequence_number ||
       last_received_zoom_level_ != visual_properties.zoom_level ||
       last_received_css_zoom_factor_ != visual_properties.css_zoom_factor) &&
      local_surface_id_ == visual_properties.local_surface_id) {
    mojo::ReportBadMessage(
        "SurfaceEmbedConnectorImpl: Resize parameters changed but the local "
        "surface ID remained unchanged.");
    return;
  }
  SynchronizeVisualProperties(visual_properties, true);
}

WebContentsImpl* SurfaceEmbedConnectorImpl::parent_web_contents() const {
  return static_cast<WebContentsImpl*>(parent_web_contents_.get());
}

void SurfaceEmbedConnectorImpl::SetView(RenderWidgetHostViewChildFrame* view,
                                        bool allow_paint_holding) {
  // Detach ourselves from the previous `view_`.
  if (view_) {
    RenderWidgetHostViewBase* root_view = GetRootRenderWidgetHostView();
    if (root_view && root_view->GetCursorManager()) {
      // TODO(surface-embed): Consider renaming this API to ViewBeingDetached if
      // view_ is not necessarily guaranteed to be destroyed.
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

    // If the child frame is already visible, it became visible before the
    // frame connector was attached. We need to retroactively update the
    // visibility of its child views.
    if (!view_->host()->IsHidden()) {
      SetVisibilityForChildViews(true);
    }

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
SurfaceEmbedConnectorImpl::GetParentRenderWidgetHostView() {
  if (!parent_web_contents_) {
    return nullptr;
  }
  return static_cast<RenderWidgetHostViewBase*>(
      parent_web_contents()->GetRenderWidgetHostView());
}

RenderWidgetHostViewBase*
SurfaceEmbedConnectorImpl::GetRootRenderWidgetHostView() {
  if (!parent_web_contents_) {
    return nullptr;
  }
  auto* root_web_contents = parent_web_contents();
  auto* root_connector = static_cast<SurfaceEmbedConnectorImpl*>(
      parent_web_contents()->GetSurfaceEmbedConnector());
  while (root_connector && root_connector->parent_web_contents()) {
    root_web_contents = root_connector->parent_web_contents();
    root_connector = static_cast<SurfaceEmbedConnectorImpl*>(
        root_connector->parent_web_contents()->GetSurfaceEmbedConnector());
  }
  CHECK(root_web_contents);
  return static_cast<RenderWidgetHostViewBase*>(
      root_web_contents->GetRenderWidgetHostView());
}

void SurfaceEmbedConnectorImpl::RenderProcessGone() {
  delegate_->ChildProcessGone();

  // TODO(crbug.com/479743223): CrossProcessFrameConnector does a lot of logging
  // and sometimes reloading here that's about child frames in the usual sense.
  // Things embedded here do not always have those semantics, but it might make
  // sense to do something parallel.
}

void SurfaceEmbedConnectorImpl::FirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {}

void SurfaceEmbedConnectorImpl::SendIntrinsicSizingInfoToParent(
    blink::mojom::IntrinsicSizingInfoPtr) {}

void SurfaceEmbedConnectorImpl::SynchronizeVisualProperties(
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

  if (!view_) {
    return;
  }

  view_->UpdateScreenInfo();

  RenderWidgetHostImpl* render_widget_host = view_->host();
  CHECK(render_widget_host);

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

void SurfaceEmbedConnectorImpl::UpdateCursor(const ui::Cursor& cursor) {}

FrameConnector::RootViewFocusState SurfaceEmbedConnectorImpl::HasFocus() {
  return RootViewFocusState::kNullView;
}

void SurfaceEmbedConnectorImpl::FocusRootView() {}

blink::mojom::PointerLockResult SurfaceEmbedConnectorImpl::LockPointer(
    bool request_unadjusted_movement) {
  return blink::mojom::PointerLockResult::kUnknownError;
}

blink::mojom::PointerLockResult SurfaceEmbedConnectorImpl::ChangePointerLock(
    bool request_unadjusted_movement) {
  return blink::mojom::PointerLockResult::kUnknownError;
}

void SurfaceEmbedConnectorImpl::UnlockPointer() {}

bool SurfaceEmbedConnectorImpl::HasSize() {
  return has_size_;
}

const display::ScreenInfos& SurfaceEmbedConnectorImpl::GetScreenInfos() {
  return screen_infos_;
}

const viz::LocalSurfaceId& SurfaceEmbedConnectorImpl::GetLocalSurfaceId() {
  return local_surface_id_;
}

const blink::mojom::ViewportIntersectionState&
SurfaceEmbedConnectorImpl::GetIntersectionState() {
  return intersection_state_;
}

uint32_t SurfaceEmbedConnectorImpl::GetCaptureSequenceNumber() {
  return capture_sequence_number_;
}

const gfx::Rect& SurfaceEmbedConnectorImpl::GetRectInParentViewInDip() {
  return rect_in_parent_view_in_dip_;
}

const gfx::Size& SurfaceEmbedConnectorImpl::GetLocalFrameSizeInDip() {
  return local_frame_size_in_dip_;
}

const gfx::Size& SurfaceEmbedConnectorImpl::GetLocalFrameSizeInPixels() {
  return local_frame_size_in_pixels_;
}

double SurfaceEmbedConnectorImpl::GetCssZoomFactor() {
  return last_received_css_zoom_factor_;
}

double SurfaceEmbedConnectorImpl::GetCssZoomFactorForTesting() {
  return last_received_css_zoom_factor_;
}

const gfx::Size&
SurfaceEmbedConnectorImpl::GetLocalFrameSizeInPixelsForTesting() {
  return local_frame_size_in_pixels_;
}

void SurfaceEmbedConnectorImpl::EnableAutoResize(const gfx::Size& min_size,
                                                 const gfx::Size& max_size) {}

void SurfaceEmbedConnectorImpl::DisableAutoResize() {}

bool SurfaceEmbedConnectorImpl::IsInert() {
  return is_inert_;
}

cc::TouchAction SurfaceEmbedConnectorImpl::InheritedEffectiveTouchAction() {
  return inherited_effective_touch_action_;
}

bool SurfaceEmbedConnectorImpl::IsHidden() {
  // We want IsHidden() to return false even when the page isn't actually
  // rendering us, since WebContents may want to render us for features like
  // capture; any CSS that's hiding us should make us not show up incorrectly
  // in the parent renderer regardless.
  return !parent_web_contents_;
}

bool SurfaceEmbedConnectorImpl::IsThrottled() {
  return is_throttled_;
}

bool SurfaceEmbedConnectorImpl::IsSubtreeThrottled() {
  return subtree_throttled_;
}

bool SurfaceEmbedConnectorImpl::IsDisplayLocked() {
  return display_locked_;
}

void SurfaceEmbedConnectorImpl::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {
  if (metadata.local_surface_id.has_value() &&
      local_surface_id_ != *metadata.local_surface_id) {
    delegate_->UpdateLocalSurfaceIdFromChild(*metadata.local_surface_id);
  }
}

void SurfaceEmbedConnectorImpl::SetVisibilityForChildViews(bool visible) {
  if (current_child_frame_host()) {
    current_child_frame_host()->SetVisibilityForChildViews(visible);
  }
}

void SurfaceEmbedConnectorImpl::SetLocalFrameSize(
    const gfx::Size& local_frame_size) {
  has_size_ = true;
  const float dsf = screen_infos_.current().device_scale_factor;
  local_frame_size_in_pixels_ = local_frame_size;
  local_frame_size_in_dip_ =
      gfx::ScaleToRoundedSize(local_frame_size, 1.f / dsf);
}

void SurfaceEmbedConnectorImpl::SetRectInParentView(
    const gfx::Rect& rect_in_parent_view) {
  const float dsf = screen_infos_.current().device_scale_factor;
  rect_in_parent_view_in_dip_ = gfx::Rect(
      gfx::ScaleToFlooredPoint(rect_in_parent_view.origin(), 1.f / dsf),
      gfx::ScaleToCeiledSize(rect_in_parent_view.size(), 1.f / dsf));

  if (view_) {
    view_->SetBounds(rect_in_parent_view_in_dip_);
  }

  // TODO(crbug.com/496266440): Notify the embedder of the rect change so that
  // it can call SendScreenRects on all subtrees rooted at the guest web
  // contents?
}

void SurfaceEmbedConnectorImpl::OnVisibilityChanged(
    blink::mojom::FrameVisibility visibility) {
  visibility_ = visibility;

  if (!view_) {
    return;
  }

  if (current_child_frame_host()) {
    current_child_frame_host()->VisibilityChanged(visibility_);
  }

  switch (visibility) {
    case blink::mojom::FrameVisibility::kRenderedInViewport:
      child_web_contents_->WasShown();
      break;
    case blink::mojom::FrameVisibility::kNotRendered:
      child_web_contents_->WasHidden();
      break;
    case blink::mojom::FrameVisibility::kRenderedOutOfViewport:
      child_web_contents_->WasOccluded();
      break;
  }
}

bool SurfaceEmbedConnectorImpl::IsVisible() {
  if (visibility_ == blink::mojom::FrameVisibility::kNotRendered ||
      GetIntersectionState().viewport_intersection.IsEmpty()) {
    return false;
  }

  if (EmbedderVisibility() != Visibility::VISIBLE) {
    return false;
  }

  return true;
}

void SurfaceEmbedConnectorImpl::DelegateWasShown() {}

Visibility SurfaceEmbedConnectorImpl::EmbedderVisibility() {
  if (!parent_web_contents()) {
    return Visibility::HIDDEN;
  }
  return parent_web_contents()->GetVisibility();
}

input::RenderWidgetHostViewInput*
SurfaceEmbedConnectorImpl::GetParentViewInput() {
  return GetParentRenderWidgetHostView();
}

input::RenderWidgetHostViewInput*
SurfaceEmbedConnectorImpl::GetRootViewInput() {
  return GetRootRenderWidgetHostView();
}

void SurfaceEmbedConnectorImpl::UpdateViewForCurrentRenderFrameHost() {
  // Should not get here without being attached to a child WebContents.
  CHECK(child_web_contents_);

  // Get the current RenderWidgetHostView for the child WebContents.
  auto* base_view = static_cast<RenderWidgetHostViewBase*>(
      child_web_contents_->GetRenderWidgetHostView());

  if (!base_view) {
    SetView(nullptr, /*allow_paint_holding=*/false);
    return;
  }

  CHECK(base_view->IsRenderWidgetHostViewChildFrame());
  auto* child_view = static_cast<RenderWidgetHostViewChildFrame*>(base_view);

  if (view_ != child_view) {
    SetView(child_view, /*allow_paint_holding=*/false);
  }
}

void SurfaceEmbedConnectorImpl::ResetRectInParentView() {
  local_surface_id_ = viz::LocalSurfaceId();
  // TODO(crbug.com/40561516): Consider whether we actually need the next 2
  // lines or not.
  rect_in_parent_view_in_dip_ = gfx::Rect();
  last_received_local_frame_size_ = gfx::Size();
}

void SurfaceEmbedConnectorImpl::OnRenderFrameCreated() {
  UpdateViewForCurrentRenderFrameHost();
}

RenderFrameHostImpl* SurfaceEmbedConnectorImpl::current_child_frame_host()
    const {
  if (!child_web_contents()) {
    return nullptr;
  }
  return static_cast<RenderFrameHostImpl*>(
      child_web_contents()->GetPrimaryMainFrame());
}

}  // namespace content
