// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/frame/web_view_frame_widget.h"

#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"

namespace blink {

WebViewFrameWidget::WebViewFrameWidget(WebWidgetClient& client,
                                       WebViewImpl& web_view)
    : WebFrameWidgetBase(client),
      web_view_(&web_view),
      self_keep_alive_(PERSISTENT_FROM_HERE, this) {
  web_view_->SetWebWidget(this);
}

WebViewFrameWidget::~WebViewFrameWidget() = default;

void WebViewFrameWidget::Close() {
  // Closing the WebViewFrameWidget happens in response to the local main frame
  // being detached from the Page/WebViewImpl.
  web_view_->SetWebWidget(nullptr);
  web_view_ = nullptr;
  WebFrameWidgetBase::Close();
  self_keep_alive_.Clear();
}

WebSize WebViewFrameWidget::Size() {
  return web_view_->Size();
}

void WebViewFrameWidget::Resize(const WebSize& size) {
  web_view_->Resize(size);
}

void WebViewFrameWidget::DidEnterFullscreen() {
  web_view_->DidEnterFullscreen();
}

void WebViewFrameWidget::DidExitFullscreen() {
  web_view_->DidExitFullscreen();
}

void WebViewFrameWidget::SetSuppressFrameRequestsWorkaroundFor704763Only(
    bool suppress_frame_requests) {
  web_view_->SetSuppressFrameRequestsWorkaroundFor704763Only(
      suppress_frame_requests);
}
void WebViewFrameWidget::BeginFrame(base::TimeTicks last_frame_time,
                                    bool record_main_frame_metrics) {
  web_view_->BeginFrame(last_frame_time, record_main_frame_metrics);
}

void WebViewFrameWidget::DidBeginFrame() {
  web_view_->DidBeginFrame();
}

void WebViewFrameWidget::BeginRafAlignedInput() {
  web_view_->BeginRafAlignedInput();
}

void WebViewFrameWidget::EndRafAlignedInput() {
  web_view_->EndRafAlignedInput();
}

void WebViewFrameWidget::BeginUpdateLayers() {
  web_view_->BeginUpdateLayers();
}

void WebViewFrameWidget::EndUpdateLayers() {
  web_view_->EndUpdateLayers();
}

void WebViewFrameWidget::BeginCommitCompositorFrame() {
  web_view_->BeginCommitCompositorFrame();
}

void WebViewFrameWidget::EndCommitCompositorFrame() {
  web_view_->EndCommitCompositorFrame();
}

void WebViewFrameWidget::RecordStartOfFrameMetrics() {
  web_view_->RecordStartOfFrameMetrics();
}

void WebViewFrameWidget::RecordEndOfFrameMetrics(
    base::TimeTicks frame_begin_time) {
  web_view_->RecordEndOfFrameMetrics(frame_begin_time);
}

std::unique_ptr<cc::BeginMainFrameMetrics>
WebViewFrameWidget::GetBeginMainFrameMetrics() {
  return web_view_->GetBeginMainFrameMetrics();
}

void WebViewFrameWidget::UpdateLifecycle(LifecycleUpdate requested_update,
                                         LifecycleUpdateReason reason) {
  web_view_->UpdateLifecycle(requested_update, reason);
}

void WebViewFrameWidget::ThemeChanged() {
  web_view_->ThemeChanged();
}

WebInputEventResult WebViewFrameWidget::HandleInputEvent(
    const WebCoalescedInputEvent& event) {
  return web_view_->HandleInputEvent(event);
}

WebInputEventResult WebViewFrameWidget::DispatchBufferedTouchEvents() {
  return web_view_->DispatchBufferedTouchEvents();
}

void WebViewFrameWidget::SetCursorVisibilityState(bool is_visible) {
  web_view_->SetCursorVisibilityState(is_visible);
}

void WebViewFrameWidget::OnFallbackCursorModeToggled(bool is_on) {
  web_view_->OnFallbackCursorModeToggled(is_on);
}

void WebViewFrameWidget::ApplyViewportChanges(
    const ApplyViewportChangesArgs& args) {
  web_view_->ApplyViewportChanges(args);
}

void WebViewFrameWidget::RecordManipulationTypeCounts(
    cc::ManipulationInfo info) {
  web_view_->RecordManipulationTypeCounts(info);
}
void WebViewFrameWidget::SendOverscrollEventFromImplSide(
    const gfx::Vector2dF& overscroll_delta,
    cc::ElementId scroll_latched_element_id) {
  web_view_->SendOverscrollEventFromImplSide(overscroll_delta,
                                             scroll_latched_element_id);
}
void WebViewFrameWidget::SendScrollEndEventFromImplSide(
    cc::ElementId scroll_latched_element_id) {
  web_view_->SendScrollEndEventFromImplSide(scroll_latched_element_id);
}

void WebViewFrameWidget::MouseCaptureLost() {
  web_view_->MouseCaptureLost();
}

void WebViewFrameWidget::SetFocus(bool enable) {
  web_view_->SetFocus(enable);
}

bool WebViewFrameWidget::SelectionBounds(WebRect& anchor,
                                         WebRect& focus) const {
  return web_view_->SelectionBounds(anchor, focus);
}

bool WebViewFrameWidget::IsAcceleratedCompositingActive() const {
  return web_view_->IsAcceleratedCompositingActive();
}

WebURL WebViewFrameWidget::GetURLForDebugTrace() {
  return web_view_->GetURLForDebugTrace();
}

void WebViewFrameWidget::DidDetachLocalFrameTree() {
  web_view_->DidDetachLocalMainFrame();
}

WebInputMethodController*
WebViewFrameWidget::GetActiveWebInputMethodController() const {
  return web_view_->GetActiveWebInputMethodController();
}

bool WebViewFrameWidget::ScrollFocusedEditableElementIntoView() {
  return web_view_->ScrollFocusedEditableElementIntoView();
}

void WebViewFrameWidget::SetAnimationHost(cc::AnimationHost* host) {
  web_view_->SetAnimationHost(host);
}

void WebViewFrameWidget::SetRootLayer(scoped_refptr<cc::Layer> layer) {
  web_view_->SetRootLayer(layer);
}

cc::AnimationHost* WebViewFrameWidget::AnimationHost() const {
  return web_view_->AnimationHost();
}

WebHitTestResult WebViewFrameWidget::HitTestResultAt(const gfx::Point& point) {
  return web_view_->HitTestResultAt(point);
}

HitTestResult WebViewFrameWidget::CoreHitTestResultAt(const gfx::Point& point) {
  return web_view_->CoreHitTestResultAt(point);
}

void WebViewFrameWidget::ZoomToFindInPageRect(
    const WebRect& rect_in_root_frame) {
  web_view_->ZoomToFindInPageRect(rect_in_root_frame);
}

void WebViewFrameWidget::Trace(blink::Visitor* visitor) {
  WebFrameWidgetBase::Trace(visitor);
}

PageWidgetEventHandler* WebViewFrameWidget::GetPageWidgetEventHandler() {
  return web_view_.get();
}

}  // namespace blink
