// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/web_view_test_proxy.h"

#include <stddef.h>
#include <stdint.h>

#include "content/shell/test_runner/accessibility_controller.h"
#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/test_runner.h"
#include "content/shell/test_runner/test_runner_for_specific_view.h"
#include "content/shell/test_runner/text_input_controller.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "content/shell/test_runner/web_test_interfaces.h"
#include "content/shell/test_runner/web_widget_test_proxy.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_view.h"

namespace test_runner {

ProxyWebWidgetClient::ProxyWebWidgetClient(
    blink::WebWidgetClient* base_class_widget_client,
    blink::WebWidgetClient* widget_test_client)
    : base_class_widget_client_(base_class_widget_client),
      widget_test_client_(widget_test_client) {}

void ProxyWebWidgetClient::DidInvalidateRect(const blink::WebRect& r) {
  base_class_widget_client_->DidInvalidateRect(r);
}
bool ProxyWebWidgetClient::AllowsBrokenNullLayerTreeView() const {
  return base_class_widget_client_->AllowsBrokenNullLayerTreeView();
}
void ProxyWebWidgetClient::ScheduleAnimation() {
  widget_test_client_->ScheduleAnimation();
}
void ProxyWebWidgetClient::IntrinsicSizingInfoChanged(
    const blink::WebIntrinsicSizingInfo& info) {
  base_class_widget_client_->IntrinsicSizingInfoChanged(info);
}
void ProxyWebWidgetClient::DidMeaningfulLayout(
    blink::WebMeaningfulLayout layout) {
  base_class_widget_client_->DidMeaningfulLayout(layout);
}
void ProxyWebWidgetClient::DidFirstLayoutAfterFinishedParsing() {
  base_class_widget_client_->DidFirstLayoutAfterFinishedParsing();
}
void ProxyWebWidgetClient::DidChangeCursor(const blink::WebCursorInfo& info) {
  base_class_widget_client_->DidChangeCursor(info);
}
void ProxyWebWidgetClient::AutoscrollStart(const blink::WebFloatPoint& p) {
  base_class_widget_client_->AutoscrollStart(p);
}
void ProxyWebWidgetClient::AutoscrollFling(
    const blink::WebFloatSize& velocity) {
  base_class_widget_client_->AutoscrollFling(velocity);
}
void ProxyWebWidgetClient::AutoscrollEnd() {
  base_class_widget_client_->AutoscrollEnd();
}
void ProxyWebWidgetClient::CloseWidgetSoon() {
  base_class_widget_client_->CloseWidgetSoon();
}
void ProxyWebWidgetClient::Show(blink::WebNavigationPolicy policy) {
  base_class_widget_client_->Show(policy);
}
blink::WebRect ProxyWebWidgetClient::WindowRect() {
  return base_class_widget_client_->WindowRect();
}
void ProxyWebWidgetClient::SetWindowRect(const blink::WebRect& r) {
  base_class_widget_client_->SetWindowRect(r);
}
blink::WebRect ProxyWebWidgetClient::ViewRect() {
  return base_class_widget_client_->ViewRect();
}
void ProxyWebWidgetClient::SetToolTipText(const blink::WebString& text,
                                          blink::WebTextDirection hint) {
  widget_test_client_->SetToolTipText(text, hint);
  base_class_widget_client_->SetToolTipText(text, hint);
}
bool ProxyWebWidgetClient::RequestPointerLock() {
  return widget_test_client_->RequestPointerLock();
}
void ProxyWebWidgetClient::RequestPointerUnlock() {
  widget_test_client_->RequestPointerUnlock();
}
bool ProxyWebWidgetClient::IsPointerLocked() {
  return widget_test_client_->IsPointerLocked();
}
void ProxyWebWidgetClient::DidHandleGestureEvent(
    const blink::WebGestureEvent& event,
    bool event_cancelled) {
  base_class_widget_client_->DidHandleGestureEvent(event, event_cancelled);
}
void ProxyWebWidgetClient::DidOverscroll(
    const blink::WebFloatSize& overscroll_delta,
    const blink::WebFloatSize& accumulated_overscroll,
    const blink::WebFloatPoint& position_in_viewport,
    const blink::WebFloatSize& velocity_in_viewport,
    const cc::OverscrollBehavior& behavior) {
  base_class_widget_client_->DidOverscroll(
      overscroll_delta, accumulated_overscroll, position_in_viewport,
      velocity_in_viewport, behavior);
}
void ProxyWebWidgetClient::HasTouchEventHandlers(bool has) {
  base_class_widget_client_->HasTouchEventHandlers(has);
}
void ProxyWebWidgetClient::SetNeedsLowLatencyInput(bool needs) {
  base_class_widget_client_->SetNeedsLowLatencyInput(needs);
}
void ProxyWebWidgetClient::RequestUnbufferedInputEvents() {
  base_class_widget_client_->RequestUnbufferedInputEvents();
}
void ProxyWebWidgetClient::SetTouchAction(blink::WebTouchAction touch_action) {
  base_class_widget_client_->SetTouchAction(touch_action);
}
void ProxyWebWidgetClient::ShowVirtualKeyboardOnElementFocus() {
  base_class_widget_client_->ShowVirtualKeyboardOnElementFocus();
}
void ProxyWebWidgetClient::ConvertViewportToWindow(blink::WebRect* rect) {
  base_class_widget_client_->ConvertViewportToWindow(rect);
}
void ProxyWebWidgetClient::ConvertWindowToViewport(blink::WebFloatRect* rect) {
  base_class_widget_client_->ConvertWindowToViewport(rect);
}
void ProxyWebWidgetClient::StartDragging(network::mojom::ReferrerPolicy policy,
                                         const blink::WebDragData& data,
                                         blink::WebDragOperationsMask mask,
                                         const SkBitmap& drag_image,
                                         const blink::WebPoint& image_offset) {
  widget_test_client_->StartDragging(policy, data, mask, drag_image,
                                     image_offset);
  // Don't forward this call to |base_class_widget_client_| because we don't
  // want to do a real drag-and-drop.
}

WebViewTestProxyBase::WebViewTestProxyBase()
    : accessibility_controller_(new AccessibilityController(this)),
      text_input_controller_(new TextInputController(this)),
      view_test_runner_(new TestRunnerForSpecificView(this)) {
  WebWidgetTestProxyBase::set_web_view_test_proxy_base(this);
}

WebViewTestProxyBase::~WebViewTestProxyBase() {
  test_interfaces_->WindowClosed(this);
  if (test_interfaces_->GetDelegate() == delegate_)
    test_interfaces_->SetDelegate(nullptr);
}

void WebViewTestProxyBase::Reset() {
  accessibility_controller_->Reset();
  // text_input_controller_ doesn't have any state to reset.
  view_test_runner_->Reset();
  WebWidgetTestProxyBase::Reset();

  for (blink::WebFrame* frame = web_view_->MainFrame(); frame;
       frame = frame->TraverseNext()) {
    if (frame->IsWebLocalFrame())
      delegate_->GetWebWidgetTestProxyBase(frame->ToWebLocalFrame())->Reset();
  }
}

void WebViewTestProxyBase::BindTo(blink::WebLocalFrame* frame) {
  accessibility_controller_->Install(frame);
  text_input_controller_->Install(frame);
  view_test_runner_->Install(frame);
}

void WebViewTestProxy::Initialize(WebTestInterfaces* interfaces,
                                  WebTestDelegate* delegate) {
  // On WebViewTestProxyBase.
  set_delegate(delegate);

  std::unique_ptr<WebWidgetTestClient> web_widget_client =
      interfaces->CreateWebWidgetTestClient(web_widget_test_proxy_base());
  view_test_client_ = interfaces->CreateWebViewTestClient(this, nullptr);
  // This uses the widget_test_client set above on WebWidgetTestProxyBase.
  proxy_widget_client_ = std::make_unique<ProxyWebWidgetClient>(
      RenderViewImpl::WidgetClient(), web_widget_client.get());

  // On WebWidgetTestProxyBase.
  // It's weird that the WebView has the proxy client, but the
  // WebWidgetProxyBase has the test one only, but that's because the
  // WebWidgetTestProxyBase does not itself use the WebWidgetClient, only its
  // subclasses do.
  web_widget_test_proxy_base()->set_widget_test_client(
      std::move(web_widget_client));

  // On WebViewTestProxyBase.
  set_test_interfaces(interfaces->GetTestInterfaces());
  test_interfaces()->WindowOpened(this);
}

blink::WebView* WebViewTestProxy::CreateView(
    blink::WebLocalFrame* creator,
    const blink::WebURLRequest& request,
    const blink::WebWindowFeatures& features,
    const blink::WebString& frame_name,
    blink::WebNavigationPolicy policy,
    bool suppress_opener,
    blink::WebSandboxFlags sandbox_flags,
    const blink::SessionStorageNamespaceId& session_storage_namespace_id) {
  if (!view_test_client_->CreateView(creator, request, features, frame_name,
                                     policy, suppress_opener, sandbox_flags,
                                     session_storage_namespace_id))
    return nullptr;
  return RenderViewImpl::CreateView(creator, request, features, frame_name,
                                    policy, suppress_opener, sandbox_flags,
                                    session_storage_namespace_id);
}

void WebViewTestProxy::PrintPage(blink::WebLocalFrame* frame) {
  view_test_client_->PrintPage(frame);
}

blink::WebString WebViewTestProxy::AcceptLanguages() {
  return view_test_client_->AcceptLanguages();
}

void WebViewTestProxy::DidFocus(blink::WebLocalFrame* calling_frame) {
  view_test_client_->DidFocus(calling_frame);
  RenderViewImpl::DidFocus(calling_frame);
}

blink::WebScreenInfo WebViewTestProxy::GetScreenInfo() {
  blink::WebScreenInfo info = RenderViewImpl::GetScreenInfo();
  blink::WebScreenInfo test_info = view_test_client_->GetScreenInfo();
  if (test_info.orientation_type != blink::kWebScreenOrientationUndefined) {
    info.orientation_type = test_info.orientation_type;
    info.orientation_angle = test_info.orientation_angle;
  }
  return info;
}

blink::WebWidgetClient* WebViewTestProxy::WidgetClient() {
  return proxy_widget_client_.get();
}

WebViewTestProxy::~WebViewTestProxy() = default;

}  // namespace test_runner
