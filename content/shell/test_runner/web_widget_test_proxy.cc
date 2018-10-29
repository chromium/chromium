// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/web_widget_test_proxy.h"

#include "content/shell/test_runner/event_sender.h"
#include "content/shell/test_runner/web_test_interfaces.h"
#include "content/shell/test_runner/web_view_test_proxy.h"

namespace test_runner {

WebWidgetTestProxyBase::WebWidgetTestProxyBase()
    : event_sender_(std::make_unique<EventSender>(this)) {}

WebWidgetTestProxyBase::~WebWidgetTestProxyBase() = default;

void WebWidgetTestProxyBase::Reset() {
  event_sender_->Reset();
}

void WebWidgetTestProxyBase::BindTo(blink::WebLocalFrame* frame) {
  event_sender_->Install(frame);
}

void WebWidgetTestProxy::Initialize(
    WebTestInterfaces* interfaces,
    blink::WebWidget* web_widget,
    content::RenderViewImpl* render_view_for_local_root) {
  // The RenderViewImpl will also be a test proxy type.
  auto* view_proxy_for_local_root =
      static_cast<WebViewTestProxy*>(render_view_for_local_root);

  // On WebWidgetTestProxyBase.
  set_web_widget(web_widget);
  set_web_view_test_proxy_base(view_proxy_for_local_root);
  set_widget_test_client(interfaces->CreateWebWidgetTestClient(this));
}

void WebWidgetTestProxy::ScheduleAnimation() {
  RenderWidget::ScheduleAnimation();
  widget_test_client()->ScheduleAnimation();
}

bool WebWidgetTestProxy::RequestPointerLock() {
  return widget_test_client()->RequestPointerLock();
}

void WebWidgetTestProxy::RequestPointerUnlock() {
  widget_test_client()->RequestPointerUnlock();
}

bool WebWidgetTestProxy::IsPointerLocked() {
  return widget_test_client()->IsPointerLocked();
}

void WebWidgetTestProxy::SetToolTipText(const blink::WebString& text,
                                        blink::WebTextDirection hint) {
  RenderWidget::SetToolTipText(text, hint);
  widget_test_client()->SetToolTipText(text, hint);
}

void WebWidgetTestProxy::StartDragging(network::mojom::ReferrerPolicy policy,
                                       const blink::WebDragData& data,
                                       blink::WebDragOperationsMask mask,
                                       const SkBitmap& drag_image,
                                       const blink::WebPoint& image_offset) {
  widget_test_client()->StartDragging(policy, data, mask, drag_image,
                                      image_offset);
  // Don't forward this call to RenderWidget because we don't want to do a
  // real drag-and-drop.
}

WebWidgetTestProxy::~WebWidgetTestProxy() = default;

}  // namespace test_runner
