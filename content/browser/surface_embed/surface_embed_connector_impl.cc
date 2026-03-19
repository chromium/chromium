// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/surface_embed/surface_embed_connector_impl.h"

#include "components/input/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/surface_embed/dummy_surface_provider.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom.h"
#include "third_party/blink/public/mojom/input/pointer_lock_result.mojom.h"
#include "ui/base/cursor/cursor.h"
#include "ui/compositor/compositor.h"

namespace content {

// static
void SurfaceEmbedConnector::Attach(WebContents* child_web_contents,
                                   WebContents* parent_web_contents,
                                   SurfaceEmbedConnector::Delegate* delegate) {
  CHECK(child_web_contents);
  CHECK(parent_web_contents);
  // Must Detach the child before re-Attaching.
  CHECK(!child_web_contents->GetSurfaceEmbedConnector());
  auto connector = base::WrapUnique(new SurfaceEmbedConnectorImpl(
      child_web_contents, parent_web_contents, delegate));
  static_cast<WebContentsImpl*>(child_web_contents)
      ->SetSurfaceEmbedConnector(std::move(connector));
}

// static
void SurfaceEmbedConnector::Detach(WebContents* child_web_contents) {
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
      parent_web_contents_(parent_web_contents->GetWeakPtr()),
      dummy_surface_provider_(std::make_unique<DummySurfaceProvider>()) {}

SurfaceEmbedConnectorImpl::~SurfaceEmbedConnectorImpl() = default;

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
  return dummy_surface_provider_->frame_sink_id();
}

void SurfaceEmbedConnectorImpl::OnSynchronizeVisualProperties(
    const blink::FrameVisualProperties& visual_properties) {
  dummy_surface_provider_->SubmitCompositorFrame(
      visual_properties.local_surface_id,
      visual_properties.screen_infos.current().device_scale_factor,
      visual_properties.local_frame_size);
}

WebContentsImpl* SurfaceEmbedConnectorImpl::parent_web_contents() const {
  return static_cast<WebContentsImpl*>(parent_web_contents_.get());
}

void SurfaceEmbedConnectorImpl::SetView(RenderWidgetHostViewChildFrame* view,
                                        bool allow_paint_holding) {
  view_ = view;
}

RenderWidgetHostViewBase*
SurfaceEmbedConnectorImpl::GetParentRenderWidgetHostView() {
  return nullptr;
}

RenderWidgetHostViewBase*
SurfaceEmbedConnectorImpl::GetRootRenderWidgetHostView() {
  return nullptr;
}

void SurfaceEmbedConnectorImpl::RenderProcessGone() {}

void SurfaceEmbedConnectorImpl::FirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {}

void SurfaceEmbedConnectorImpl::SendIntrinsicSizingInfoToParent(
    blink::mojom::IntrinsicSizingInfoPtr) {}

void SurfaceEmbedConnectorImpl::SynchronizeVisualProperties(
    const blink::FrameVisualProperties& visual_properties,
    bool propagate) {}

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
  return false;
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

void SurfaceEmbedConnectorImpl::EnableAutoResize(const gfx::Size& min_size,
                                                 const gfx::Size& max_size) {}

void SurfaceEmbedConnectorImpl::DisableAutoResize() {}

bool SurfaceEmbedConnectorImpl::IsInert() {
  return false;
}

cc::TouchAction SurfaceEmbedConnectorImpl::InheritedEffectiveTouchAction() {
  return cc::TouchAction::kAuto;
}

bool SurfaceEmbedConnectorImpl::IsHidden() {
  return false;
}

bool SurfaceEmbedConnectorImpl::IsThrottled() {
  return false;
}

bool SurfaceEmbedConnectorImpl::IsSubtreeThrottled() {
  return false;
}

bool SurfaceEmbedConnectorImpl::IsDisplayLocked() {
  return false;
}

void SurfaceEmbedConnectorImpl::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {}

void SurfaceEmbedConnectorImpl::SetVisibilityForChildViews(bool visible) {}

void SurfaceEmbedConnectorImpl::SetLocalFrameSize(
    const gfx::Size& local_frame_size) {}

void SurfaceEmbedConnectorImpl::SetRectInParentView(
    const gfx::Rect& rect_in_parent_view) {}

void SurfaceEmbedConnectorImpl::OnVisibilityChanged(
    blink::mojom::FrameVisibility visibility) {
  visibility_ = visibility;

  // TODO(surface-embed): If there is a view, propagate the change in
  // visibility to the current child render frame host and the child web
  // contents.
}

bool SurfaceEmbedConnectorImpl::IsVisible() {
  return true;
}

void SurfaceEmbedConnectorImpl::DelegateWasShown() {}

Visibility SurfaceEmbedConnectorImpl::EmbedderVisibility() {
  return Visibility::VISIBLE;
}

input::RenderWidgetHostViewInput*
SurfaceEmbedConnectorImpl::GetParentViewInput() {
  return nullptr;
}

input::RenderWidgetHostViewInput*
SurfaceEmbedConnectorImpl::GetRootViewInput() {
  return nullptr;
}

}  // namespace content
