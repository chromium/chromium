// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/guest_frame_impl.h"

#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/secure_embed_delegate.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom.h"
#include "third_party/blink/public/mojom/frame/viewport_intersection_state.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace content {

// static
std::unique_ptr<GuestFrame> GuestFrame::Create(WebContents* guest_web_contents) {
  return std::make_unique<GuestFrameImpl>(guest_web_contents);
}

GuestFrameImpl::GuestFrameImpl(WebContents* guest_web_contents)
    : guest_web_contents_(guest_web_contents) {
  auto* base_view = static_cast<RenderWidgetHostViewBase*>(
      guest_web_contents->GetRenderWidgetHostView());
  CHECK(base_view->IsRenderWidgetHostViewChildFrame());
  view_ = static_cast<RenderWidgetHostViewChildFrame*>(base_view);
  view_->SetFrameConnector(this);
  frame_sink_id_ = view_->GetFrameSinkId();
}

GuestFrameImpl::~GuestFrameImpl() = default;

void GuestFrameImpl::SetLocalSurfaceId(
    const viz::LocalSurfaceId& local_surface_id) {
  local_surface_id_ = local_surface_id;
  LOG(INFO) << "GuestFrameImpl::SetLocalSurfaceId:"
            << local_surface_id_.ToString();
  RenderWidgetHostImpl* render_widget_host = view_->host();
  render_widget_host->SendScreenRects();
  // TODO(secure-embed): The visual properties should not be hardcoded.
  render_widget_host->SetVisualPropertiesFromParentFrame(
      1.0, 1.0, false, gfx::Size(1000, 1000), gfx::Rect(10, 10, 1000, 1000),
      {});
  render_widget_host->UpdateVisualProperties(true);
}

const viz::FrameSinkId& GuestFrameImpl::GetFrameSinkId() const {
  return frame_sink_id_;
}

void GuestFrameImpl::SetView(RenderWidgetHostViewChildFrame* view,
                             bool allow_paint_holding) {
  // TODO(secure-embed): This doesn't handle clearing the view.
  view_ = view;
  view_->SetFrameConnector(this);
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
  NOTIMPLEMENTED();
}

void GuestFrameImpl::UpdateCursor(const ui::Cursor& cursor) {
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
  return true;
}

const display::ScreenInfos& GuestFrameImpl::GetScreenInfos() const {
  // TODO(secure-embed): Shouldn't need to recompute it, but I am not
  // sure when is the right time to ocmpute it.
  screen_infos_ = const_cast<GuestFrameImpl*>(this)
                      ->GetRootRenderWidgetHostView()
                      ->GetScreenInfos();
  return screen_infos_;
}

const viz::LocalSurfaceId& GuestFrameImpl::GetLocalSurfaceId() const {
  return local_surface_id_;
}

const blink::mojom::ViewportIntersectionState&
GuestFrameImpl::GetIntersectionState() const {
  NOTIMPLEMENTED();
  static const base::NoDestructor<blink::mojom::ViewportIntersectionState>
      intersection_state;
  return *intersection_state;
}

uint32_t GuestFrameImpl::GetCaptureSequenceNumber() const {
  NOTIMPLEMENTED();
  return 0;
}

const gfx::Rect& GuestFrameImpl::GetRectInParentViewInDip() const {
  NOTIMPLEMENTED();
  static const gfx::Rect rect(50, 50, 1000, 1000);
  return rect;
}

const gfx::Size& GuestFrameImpl::GetLocalFrameSizeInDip() const {
  NOTIMPLEMENTED();
  static const gfx::Size size(1000, 1000);
  return size;
}

const gfx::Size& GuestFrameImpl::GetLocalFrameSizeInPixels() const {
  NOTIMPLEMENTED();
  static const gfx::Size size(1250, 1250);
  return size;
}

double GuestFrameImpl::GetCssZoomFactor() const {
  NOTIMPLEMENTED();
  return 1.0;
}

void GuestFrameImpl::EnableAutoResize(const gfx::Size& min_size,
                                      const gfx::Size& max_size) {
  NOTIMPLEMENTED();
}

void GuestFrameImpl::DisableAutoResize() {
  NOTIMPLEMENTED();
}

bool GuestFrameImpl::IsInert() const {
  NOTIMPLEMENTED();
  return false;
}

cc::TouchAction GuestFrameImpl::InheritedEffectiveTouchAction() const {
  NOTIMPLEMENTED();
  return cc::TouchAction::kAuto;
}

bool GuestFrameImpl::IsHidden() const {
  NOTIMPLEMENTED();
  return false;
}

bool GuestFrameImpl::IsThrottled() const {
  NOTIMPLEMENTED();
  return false;
}

bool GuestFrameImpl::IsSubtreeThrottled() const {
  NOTIMPLEMENTED();
  return false;
}

bool GuestFrameImpl::IsDisplayLocked() const {
  NOTIMPLEMENTED();
  return false;
}

void GuestFrameImpl::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {
  NOTIMPLEMENTED();
}

void GuestFrameImpl::SetVisibilityForChildViews(bool visible) const {
  NOTIMPLEMENTED();
}

void GuestFrameImpl::SetLocalFrameSize(const gfx::Size& local_frame_size) {
  NOTIMPLEMENTED();
}

void GuestFrameImpl::SetRectInParentView(const gfx::Rect& rect_in_parent_view) {
  NOTIMPLEMENTED();
}

void GuestFrameImpl::SetIsInert(bool inert) {
  NOTIMPLEMENTED();
}

void GuestFrameImpl::OnSetInheritedEffectiveTouchAction(cc::TouchAction) {
  NOTIMPLEMENTED();
}

void GuestFrameImpl::OnVisibilityChanged(
    blink::mojom::FrameVisibility visibility) {
  NOTIMPLEMENTED();
}

void GuestFrameImpl::UpdateRenderThrottlingStatus(bool is_throttled,
                                                  bool subtree_throttled,
                                                  bool display_locked) {
  NOTIMPLEMENTED();
}

void GuestFrameImpl::UpdateViewportIntersection(
    const blink::mojom::ViewportIntersectionState& intersection_state,
    const std::optional<blink::FrameVisualProperties>& visual_properties) {
  NOTIMPLEMENTED();
}

bool GuestFrameImpl::IsVisible() {
  NOTIMPLEMENTED();
  return false;
}

void GuestFrameImpl::DelegateWasShown() {
  NOTIMPLEMENTED();
}

void GuestFrameImpl::OnSynchronizeVisualProperties(
    const blink::FrameVisualProperties& visual_properties) {
  NOTIMPLEMENTED();
}

Visibility GuestFrameImpl::EmbedderVisibility() {
  NOTIMPLEMENTED();
  return Visibility::HIDDEN;
}

input::RenderWidgetHostViewInput* GuestFrameImpl::GetParentViewInput() {
  NOTIMPLEMENTED();
  return nullptr;
}

input::RenderWidgetHostViewInput* GuestFrameImpl::GetRootViewInput() {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace content
