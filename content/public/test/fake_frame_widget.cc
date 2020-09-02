// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/fake_frame_widget.h"

namespace content {

FakeFrameWidget::FakeFrameWidget(
    mojo::PendingAssociatedReceiver<blink::mojom::FrameWidget> frame_widget)
    : receiver_(this, std::move(frame_widget)) {}

FakeFrameWidget::~FakeFrameWidget() = default;

base::i18n::TextDirection FakeFrameWidget::GetTextDirection() const {
  return text_direction_;
}

void FakeFrameWidget::SetTextDirection(base::i18n::TextDirection direction) {
  text_direction_ = direction;
}

#if defined(OS_MAC)
void FakeFrameWidget::GetStringAtPoint(const gfx::Point& point_in_local_root,
                                       GetStringAtPointCallback callback) {
  std::move(callback).Run(nullptr, gfx::Point());
}
#endif

base::Optional<bool> FakeFrameWidget::GetActive() const {
  return active_;
}

void FakeFrameWidget::SetActive(bool active) {
  active_ = active;
}

}  // namespace content
