// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "render_widget_host_ns_view_client_helper.h"

#include "content/browser/renderer_host/input/web_input_event_builders_mac.h"
#include "content/common/render_widget_host_ns_view.mojom.h"
#include "content/public/browser/native_web_keyboard_event.h"

namespace content {

namespace {

class ForwardingClientHelper : public RenderWidgetHostNSViewClientHelper {
 public:
  explicit ForwardingClientHelper(mojom::RenderWidgetHostNSViewClient* client)
      : client_(client) {}

 private:
  std::unique_ptr<InputEvent> TranslateEvent(
      const blink::WebInputEvent& web_event) {
    return std::make_unique<InputEvent>(web_event, ui::LatencyInfo());
  }

  // RenderWidgetHostNSViewClientHelper implementation.
  BrowserAccessibilityManager* GetRootBrowserAccessibilityManager() override {
    return nullptr;
  }
  void ForwardKeyboardEvent(const NativeWebKeyboardEvent& key_event,
                            const ui::LatencyInfo& latency_info) override {
    const blink::WebKeyboardEvent* web_event =
        static_cast<const blink::WebKeyboardEvent*>(&key_event);
    std::unique_ptr<InputEvent> input_event =
        std::make_unique<InputEvent>(*web_event, latency_info);
    client_->ForwardKeyboardEvent(std::move(input_event),
                                  key_event.skip_in_browser);
  }
  void ForwardKeyboardEventWithCommands(
      const NativeWebKeyboardEvent& key_event,
      const ui::LatencyInfo& latency_info,
      const std::vector<EditCommand>& commands) override {
    const blink::WebKeyboardEvent* web_event =
        static_cast<const blink::WebKeyboardEvent*>(&key_event);
    std::unique_ptr<InputEvent> input_event =
        std::make_unique<InputEvent>(*web_event, latency_info);
    client_->ForwardKeyboardEventWithCommands(
        std::move(input_event), key_event.skip_in_browser, commands);
  }
  void RouteOrProcessMouseEvent(
      const blink::WebMouseEvent& web_event) override {
    client_->RouteOrProcessMouseEvent(TranslateEvent(web_event));
  }
  void RouteOrProcessTouchEvent(
      const blink::WebTouchEvent& web_event) override {
    client_->RouteOrProcessTouchEvent(TranslateEvent(web_event));
  }
  void RouteOrProcessWheelEvent(
      const blink::WebMouseWheelEvent& web_event) override {
    client_->RouteOrProcessWheelEvent(TranslateEvent(web_event));
  }
  void ForwardMouseEvent(const blink::WebMouseEvent& web_event) override {
    client_->ForwardMouseEvent(TranslateEvent(web_event));
  }
  void ForwardWheelEvent(const blink::WebMouseWheelEvent& web_event) override {
    client_->ForwardWheelEvent(TranslateEvent(web_event));
  }
  void GestureBegin(blink::WebGestureEvent begin_event,
                    bool is_synthetically_injected) override {
    // The gesture type is not yet known, but assign a type to avoid
    // serialization asserts (the type will be stripped on the other side).
    begin_event.SetType(blink::WebInputEvent::kGestureScrollBegin);
    client_->GestureBegin(TranslateEvent(begin_event),
                          is_synthetically_injected);
  }
  void GestureUpdate(blink::WebGestureEvent update_event) override {
    client_->GestureUpdate(TranslateEvent(update_event));
  }
  void GestureEnd(blink::WebGestureEvent end_event) override {
    client_->GestureEnd(TranslateEvent(end_event));
  }
  void SmartMagnify(const blink::WebGestureEvent& web_event) override {
    client_->SmartMagnify(TranslateEvent(web_event));
  }

  mojom::RenderWidgetHostNSViewClient* client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ForwardingClientHelper);
};

}  // namespace

// static
std::unique_ptr<RenderWidgetHostNSViewClientHelper>
RenderWidgetHostNSViewClientHelper::CreateForMojoClient(
    content::mojom::RenderWidgetHostNSViewClient* client) {
  return std::make_unique<ForwardingClientHelper>(client);
}

}  // namespace content
