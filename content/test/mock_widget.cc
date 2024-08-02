// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_widget.h"

namespace content {

MockWidget::MockWidget() = default;

MockWidget::~MockWidget() = default;

mojo::PendingAssociatedRemote<blink::mojom::Widget> MockWidget::GetNewRemote() {
  blink_widget_.reset();
  input_receiver_.reset();
  return blink_widget_.BindNewEndpointAndPassDedicatedRemote();
}

const std::vector<blink::VisualProperties>&
MockWidget::ReceivedVisualProperties() {
  return visual_properties_;
}

void MockWidget::ClearVisualProperties() {
  visual_properties_.clear();
}

const std::vector<std::pair<gfx::Rect, gfx::Rect>>&
MockWidget::ReceivedScreenRects() {
  return screen_rects_;
}

void MockWidget::ClearScreenRects() {
  for (auto& callback : screen_rects_callbacks_) {
    std::move(callback).Run();
  }
  screen_rects_callbacks_.clear();
  screen_rects_.clear();
}

void MockWidget::GetWidgetInputHandler(
    mojo::PendingReceiver<blink::mojom::WidgetInputHandler> request,
    mojo::PendingRemote<blink::mojom::WidgetInputHandlerHost> host) {
  // Some tests try to reinitialize a host against same MockWidget multiple
  // times. We assume this happens against the same host and avoid changing the
  // binding.
  if (!input_handler_host_.is_bound())
    input_handler_host_.Bind(std::move(host));
}

void MockWidget::ForceRedraw(ForceRedrawCallback callback) {}

void MockWidget::SetTouchActionFromMain(cc::TouchAction touch_action) {
  input_handler_host_->SetTouchActionFromMain(touch_action);
}

void MockWidget::UpdateVisualProperties(
    const blink::VisualProperties& visual_properties) {
  visual_properties_.push_back(visual_properties);
}

void MockWidget::UpdateScreenRects(const gfx::Rect& widget_screen_rect,
                                   const gfx::Rect& window_screen_rect,
                                   UpdateScreenRectsCallback callback) {
  screen_rects_.push_back(
      std::make_pair(widget_screen_rect, window_screen_rect));
  screen_rects_callbacks_.push_back(std::move(callback));
}

void MockWidget::WasHidden() {
  is_hidden_ = true;
  if (shown_hidden_callback_)
    std::move(shown_hidden_callback_).Run();
}

void MockWidget::WasShown(bool was_evicted,
                          blink::mojom::RecordContentToVisibleTimeRequestPtr
                              record_tab_switch_time_request) {
  is_hidden_ = false;
  if (shown_hidden_callback_)
    std::move(shown_hidden_callback_).Run();
}

void MockWidget::RequestSuccessfulPresentationTimeForNextFrame(
    blink::mojom::RecordContentToVisibleTimeRequestPtr visible_time_request) {}

void MockWidget::CancelSuccessfulPresentationTimeRequest() {}

void MockWidget::SetupRenderInputRouterConnections(
    mojo::PendingReceiver<blink::mojom::RenderInputRouterClient>
        browser_request,
    mojo::PendingReceiver<blink::mojom::RenderInputRouterClient> viz_request) {
  input_receiver_.Bind(std::move(browser_request));
}

}  // namespace content
