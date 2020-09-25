// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_FAKE_FRAME_WIDGET_H_
#define CONTENT_PUBLIC_TEST_FAKE_FRAME_WIDGET_H_

#include "base/i18n/rtl.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/page/drag.mojom.h"
#include "third_party/blink/public/mojom/page/widget.mojom.h"
#include "ui/base/ui_base_types.h"

#if defined(OS_MAC)
#include "ui/base/mojom/attributed_string.mojom.h"
#endif

namespace content {

class FakeFrameWidget : public blink::mojom::FrameWidget {
 public:
  explicit FakeFrameWidget(
      mojo::PendingAssociatedReceiver<blink::mojom::FrameWidget> frame_widget);
  ~FakeFrameWidget() override;

  FakeFrameWidget(const FakeFrameWidget&) = delete;
  void operator=(const FakeFrameWidget&) = delete;

  base::i18n::TextDirection GetTextDirection() const;
  base::Optional<bool> GetActive() const;

 private:
  void DragTargetDragOver(const gfx::PointF& point_in_viewport,
                          const gfx::PointF& screen_point,
                          blink::DragOperationsMask operations_allowed,
                          uint32_t modifiers,
                          DragTargetDragOverCallback callback) override {}
  void DragTargetDragLeave(const gfx::PointF& point_in_viewport,
                           const gfx::PointF& screen_point) override {}
  void DragTargetDrop(blink::mojom::DragDataPtr drag_data,
                      const gfx::PointF& point_in_viewport,
                      const gfx::PointF& screen_point,
                      uint32_t key_modifiers) override {}
  void DragSourceEndedAt(const gfx::PointF& client_point,
                         const gfx::PointF& screen_point,
                         blink::DragOperation operation) override {}
  void DragSourceSystemDragEnded() override {}
  void SetBackgroundOpaque(bool value) override {}
  void SetTextDirection(base::i18n::TextDirection direction) override;
  void SetActive(bool active) override;
  void SetInheritedEffectiveTouchActionForSubFrame(
      const cc::TouchAction touch_action) override {}
  void UpdateRenderThrottlingStatusForSubFrame(
      bool is_throttled,
      bool subtree_throttled) override {}
  void SetIsInertForSubFrame(bool inert) override {}
#if defined(OS_MAC)
  void GetStringAtPoint(const gfx::Point& point_in_local_root,
                        GetStringAtPointCallback callback) override;
#endif
  void ShowContextMenu(ui::MenuSourceType source_type,
                       const gfx::Point& location) override {}
  void EnableDeviceEmulation(
      const blink::DeviceEmulationParams& parameters) override {}
  void DisableDeviceEmulation() override {}
  void BindWidgetCompositor(
      mojo::PendingReceiver<blink::mojom::WidgetCompositor> receiver) override {
  }
  void BindInputTargetClient(
      mojo::PendingReceiver<viz::mojom::InputTargetClient> receiver) override {}

  mojo::AssociatedReceiver<blink::mojom::FrameWidget> receiver_;
  base::i18n::TextDirection text_direction_ =
      base::i18n::TextDirection::UNKNOWN_DIRECTION;
  base::Optional<bool> active_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FAKE_FRAME_WIDGET_H_
