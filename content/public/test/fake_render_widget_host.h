// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_FAKE_RENDER_WIDGET_HOST_H_
#define CONTENT_PUBLIC_TEST_FAKE_RENDER_WIDGET_HOST_H_

#include <utility>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom-forward.h"
#include "third_party/blink/public/mojom/input/touch_event.mojom-forward.h"
#include "third_party/blink/public/mojom/page/widget.mojom.h"
#include "ui/base/ime/mojom/text_input_state.mojom.h"

namespace content {

class FakeRenderWidgetHost : public blink::mojom::FrameWidgetHost,
                             public blink::mojom::WidgetHost,
                             public blink::mojom::WidgetInputHandlerHost {
 public:
  FakeRenderWidgetHost();
  ~FakeRenderWidgetHost() override;

  std::pair<mojo::PendingAssociatedRemote<blink::mojom::FrameWidgetHost>,
            mojo::PendingAssociatedReceiver<blink::mojom::FrameWidget>>
  BindNewFrameWidgetInterfaces();

  std::pair<mojo::PendingAssociatedRemote<blink::mojom::WidgetHost>,
            mojo::PendingAssociatedReceiver<blink::mojom::Widget>>
  BindNewWidgetInterfaces();

  // blink::mojom::FrameWidgetHost overrides.
  void AnimateDoubleTapZoomInMainFrame(const gfx::Point& tap_point,
                                       const gfx::Rect& rect_to_zoom) override;
  void ZoomToFindInPageRectInMainFrame(const gfx::Rect& rect_to_zoom) override;
  void SetHasTouchEventConsumers(
      blink::mojom::TouchEventConsumersPtr consumers) override;
  void IntrinsicSizingInfoChanged(
      blink::mojom::IntrinsicSizingInfoPtr sizing_info) override;
  void AutoscrollStart(const gfx::PointF& position) override;
  void AutoscrollFling(const gfx::Vector2dF& position) override;
  void AutoscrollEnd() override;
  void DidFirstVisuallyNonEmptyPaint() override;
  void StartDragging(blink::mojom::DragDataPtr drag_data,
                     blink::DragOperationsMask operations_allowed,
                     const SkBitmap& bitmap,
                     const gfx::Vector2d& bitmap_offset_in_dip,
                     blink::mojom::DragEventSourceInfoPtr event_info) override;

  // blink::mojom::WidgetHost overrides.
  void SetCursor(const ui::Cursor& cursor) override;
  void SetToolTipText(const base::string16& tooltip_text,
                      base::i18n::TextDirection text_direction_hint) override;
  void TextInputStateChanged(ui::mojom::TextInputStatePtr state) override;
  void SelectionBoundsChanged(const gfx::Rect& anchor_rect,
                              base::i18n::TextDirection anchor_dir,
                              const gfx::Rect& focus_rect,
                              base::i18n::TextDirection focus_dir,
                              bool is_anchor_first) override;

  // blink::mojom::WidgetInputHandlerHost overrides.
  void SetTouchActionFromMain(cc::TouchAction touch_action) override;
  void DidOverscroll(blink::mojom::DidOverscrollParamsPtr params) override;
  void DidStartScrollingViewport() override;
  void ImeCancelComposition() override;
  void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& bounds) override;
  void SetMouseCapture(bool capture) override;
  void RequestMouseLock(bool from_user_gesture,
                        bool privileged,
                        bool unadjusted_movement,
                        RequestMouseLockCallback callback) override;

  mojo::AssociatedReceiver<blink::mojom::WidgetHost>&
  widget_host_receiver_for_testing() {
    return widget_host_receiver_;
  }

  mojo::AssociatedRemote<blink::mojom::Widget>& widget_remote_for_testing() {
    return widget_remote_;
  }

  blink::mojom::WidgetInputHandler* GetWidgetInputHandler();
  blink::mojom::FrameWidgetInputHandler* GetFrameWidgetInputHandler();

  gfx::Range LastCompositionRange() const { return last_composition_range_; }
  const std::vector<gfx::Rect>& LastCompositionBounds() const {
    return last_composition_bounds_;
  }

 private:
  gfx::Range last_composition_range_;
  std::vector<gfx::Rect> last_composition_bounds_;
  mojo::AssociatedReceiver<blink::mojom::FrameWidgetHost>
      frame_widget_host_receiver_{this};
  mojo::AssociatedRemote<blink::mojom::FrameWidget> frame_widget_remote_;
  mojo::AssociatedReceiver<blink::mojom::WidgetHost> widget_host_receiver_{
      this};
  mojo::AssociatedRemote<blink::mojom::Widget> widget_remote_;
  mojo::Remote<blink::mojom::WidgetInputHandler> widget_input_handler_;
  mojo::Receiver<blink::mojom::WidgetInputHandlerHost>
      widget_input_handler_host_{this};
  mojo::AssociatedRemote<blink::mojom::FrameWidgetInputHandler>
      frame_widget_input_handler_;

  DISALLOW_COPY_AND_ASSIGN(FakeRenderWidgetHost);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FAKE_RENDER_WIDGET_HOST_H_
