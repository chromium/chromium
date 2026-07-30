// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/fake_frame_widget.h"

#include "build/build_config.h"

namespace content {

FakeFrameWidget::FakeFrameWidget(
    mojo::PendingAssociatedReceiver<blink::mojom::FrameWidget> frame_widget)
    : receiver_(this, std::move(frame_widget)),
      intersection_state_(blink::mojom::ViewportIntersectionState::New()) {}

FakeFrameWidget::~FakeFrameWidget() = default;

base::i18n::TextDirection FakeFrameWidget::GetTextDirection() const {
  return text_direction_;
}

void FakeFrameWidget::SetTextDirection(base::i18n::TextDirection direction) {
  text_direction_ = direction;
}

#if BUILDFLAG(IS_MAC)
void FakeFrameWidget::GetStringAtPoint(const gfx::Point& point_in_local_root,
                                       GetStringAtPointCallback callback) {
  std::move(callback).Run(nullptr, gfx::Point());
}
#endif

std::optional<bool> FakeFrameWidget::GetActive() const {
  return active_;
}

void FakeFrameWidget::SetActive(bool active) {
  active_ = active;
}

const blink::mojom::ViewportIntersectionStatePtr&
FakeFrameWidget::GetIntersectionState() const {
  return intersection_state_;
}

void FakeFrameWidget::SetViewportIntersection(
    blink::mojom::ViewportIntersectionStatePtr intersection_state,
    const std::optional<blink::VisualProperties>& visual_properties) {
  intersection_state_ = std::move(intersection_state);
}

void FakeFrameWidget::UpdateRenderThrottlingStatusForSubFrame(
    bool is_throttled,
    bool subtree_throttled,
    bool display_locked) {
  is_throttled_ = is_throttled;
  subtree_throttled_ = subtree_throttled;
  display_locked_ = display_locked;
}

std::optional<bool> FakeFrameWidget::IsThrottled() const {
  return is_throttled_;
}

std::optional<bool> FakeFrameWidget::IsSubtreeThrottled() const {
  return subtree_throttled_;
}

std::optional<bool> FakeFrameWidget::IsDisplayLocked() const {
  return display_locked_;
}

}  // namespace content
