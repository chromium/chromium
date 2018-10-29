// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/web_widget_test_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "content/shell/test_runner/event_sender.h"
#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/test_runner.h"
#include "content/shell/test_runner/test_runner_for_specific_view.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "content/shell/test_runner/web_view_test_proxy.h"
#include "content/shell/test_runner/web_widget_test_proxy.h"
#include "third_party/blink/public/platform/web_screen_info.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_widget.h"

namespace test_runner {

WebWidgetTestClient::WebWidgetTestClient(
    WebWidgetTestProxyBase* web_widget_test_proxy_base)
    : web_widget_test_proxy_base_(web_widget_test_proxy_base),
      animation_scheduled_(false),
      weak_factory_(this) {
  DCHECK(web_widget_test_proxy_base_);
}

WebWidgetTestClient::~WebWidgetTestClient() {}

void WebWidgetTestClient::ScheduleAnimation() {
  if (!test_runner()->TestIsRunning())
    return;

  if (!animation_scheduled_) {
    animation_scheduled_ = true;
    delegate()->PostDelayedTask(base::BindOnce(&WebWidgetTestClient::AnimateNow,
                                               weak_factory_.GetWeakPtr()),
                                base::TimeDelta::FromMilliseconds(1));
  }
}

void WebWidgetTestClient::AnimateNow() {
  if (!animation_scheduled_)
    return;

  animation_scheduled_ = false;
  bool animation_requires_raster = test_runner()->animation_requires_raster();
  blink::WebWidget* web_widget = web_widget_test_proxy_base_->web_widget();
  web_widget->UpdateAllLifecyclePhasesAndCompositeForTesting(
      animation_requires_raster);
  if (blink::WebPagePopup* popup = web_widget->GetPagePopup())
    popup->UpdateAllLifecyclePhasesAndCompositeForTesting(
        animation_requires_raster);
}

bool WebWidgetTestClient::RequestPointerLock() {
  return view_test_runner()->RequestPointerLock();
}

void WebWidgetTestClient::RequestPointerUnlock() {
  view_test_runner()->RequestPointerUnlock();
}

bool WebWidgetTestClient::IsPointerLocked() {
  return view_test_runner()->isPointerLocked();
}

void WebWidgetTestClient::SetToolTipText(const blink::WebString& text,
                                         blink::WebTextDirection direction) {
  test_runner()->setToolTipText(text);
}

void WebWidgetTestClient::StartDragging(network::mojom::ReferrerPolicy policy,
                                        const blink::WebDragData& data,
                                        blink::WebDragOperationsMask mask,
                                        const SkBitmap& drag_image,
                                        const blink::WebPoint& image_offset) {
  test_runner()->setDragImage(drag_image);

  // When running a test, we need to fake a drag drop operation otherwise
  // Windows waits for real mouse events to know when the drag is over.
  web_widget_test_proxy_base_->event_sender()->DoDragDrop(data, mask);
}

bool WebWidgetTestClient::AllowsBrokenNullLayerTreeView() const {
  // This call should go to the production client, not here.
  NOTREACHED();
  return false;
}

TestRunnerForSpecificView* WebWidgetTestClient::view_test_runner() {
  return web_widget_test_proxy_base_->web_view_test_proxy_base()
      ->view_test_runner();
}

WebTestDelegate* WebWidgetTestClient::delegate() {
  return web_widget_test_proxy_base_->web_view_test_proxy_base()->delegate();
}

TestRunner* WebWidgetTestClient::test_runner() {
  return web_widget_test_proxy_base_->web_view_test_proxy_base()
      ->test_interfaces()
      ->GetTestRunner();
}

}  // namespace test_runner
