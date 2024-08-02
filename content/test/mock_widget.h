// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_WIDGET_H_
#define CONTENT_TEST_MOCK_WIDGET_H_

#include <stddef.h>

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/widget/visual_properties.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom.h"
#include "third_party/blink/public/mojom/widget/platform_widget.mojom.h"
#include "third_party/blink/public/mojom/widget/record_content_to_visible_time_request.mojom.h"

namespace content {

class MockWidget : public blink::mojom::Widget,
                   public blink::mojom::RenderInputRouterClient {
 public:
  MockWidget();

  ~MockWidget() override;

  mojo::PendingAssociatedRemote<blink::mojom::Widget> GetNewRemote();
  const std::vector<blink::VisualProperties>& ReceivedVisualProperties();
  void ClearVisualProperties();

  const std::vector<std::pair<gfx::Rect, gfx::Rect>>& ReceivedScreenRects();
  void ClearScreenRects();
  void SetTouchActionFromMain(cc::TouchAction touch_action);

  // Invoked when this widget is shown or hidden.
  void SetShownHiddenCallback(base::RepeatingClosure callback) {
    shown_hidden_callback_ = std::move(callback);
  }

  void ClearHidden() { is_hidden_ = std::nullopt; }
  const std::optional<bool>& IsHidden() const { return is_hidden_; }

  // blink::mojom::RenderInputRouterClient overrides;
  void GetWidgetInputHandler(
      mojo::PendingReceiver<blink::mojom::WidgetInputHandler> request,
      mojo::PendingRemote<blink::mojom::WidgetInputHandlerHost> host) override;
  void ShowContextMenu(ui::MenuSourceType source_type,
                       const gfx::Point& location) override {}
  void BindInputTargetClient(
      mojo::PendingReceiver<viz::mojom::InputTargetClient> receiver) override {}

  // blink::mojom::Widget overrides.
  void ForceRedraw(ForceRedrawCallback callback) override;
  void UpdateVisualProperties(
      const blink::VisualProperties& visual_properties) override;

  void UpdateScreenRects(const gfx::Rect& widget_screen_rect,
                         const gfx::Rect& window_screen_rect,
                         UpdateScreenRectsCallback callback) override;
  void WasHidden() override;
  void WasShown(bool was_evicted,
                blink::mojom::RecordContentToVisibleTimeRequestPtr
                    record_tab_switch_time_request) override;
  void RequestSuccessfulPresentationTimeForNextFrame(
      blink::mojom::RecordContentToVisibleTimeRequestPtr visible_time_request)
      override;
  void CancelSuccessfulPresentationTimeRequest() override;
  void SetupRenderInputRouterConnections(
      mojo::PendingReceiver<blink::mojom::RenderInputRouterClient>
          browser_request,
      mojo::PendingReceiver<blink::mojom::RenderInputRouterClient> viz_request)
      override;

 private:
  std::optional<bool> is_hidden_;
  base::RepeatingClosure shown_hidden_callback_;
  std::vector<blink::VisualProperties> visual_properties_;
  std::vector<std::pair<gfx::Rect, gfx::Rect>> screen_rects_;
  std::vector<UpdateScreenRectsCallback> screen_rects_callbacks_;
  mojo::Receiver<blink::mojom::RenderInputRouterClient> input_receiver_{this};
  mojo::Remote<blink::mojom::WidgetInputHandlerHost> input_handler_host_;
  mojo::AssociatedReceiver<blink::mojom::Widget> blink_widget_{this};
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_WIDGET_H_
