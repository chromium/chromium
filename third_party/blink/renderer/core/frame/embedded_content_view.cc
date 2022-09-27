// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/embedded_content_view.h"

#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"

namespace blink {

void EmbeddedContentView::SetFrameRect(const gfx::Rect& frame_rect) {
  if (frame_rect == frame_rect_)
    return;
  gfx::Rect old_rect = frame_rect_;
  frame_rect_ = frame_rect;
  FrameRectsChanged(old_rect);
}

gfx::Point EmbeddedContentView::Location() const {
  gfx::Point location(frame_rect_.origin());

  // As an optimization, we don't include the root layer's scroll offset in the
  // frame rect.  As a result, we don't need to recalculate the frame rect every
  // time the root layer scrolls, but we need to add it in here.
  LayoutEmbeddedContent* owner = GetLayoutEmbeddedContent();
  if (owner) {
    LayoutView* owner_layout_view = owner->View();
    DCHECK(owner_layout_view);
    if (owner_layout_view->IsScrollContainer()) {
      // Floored because the frame_rect in a content view is an gfx::Rect. We
      // may want to reevaluate that since scroll offsets/layout can be
      // fractional.
      location -= ToFlooredVector2d(owner_layout_view->ScrolledContentOffset());
    }
  }
  return location;
}

void EmbeddedContentView::SetSelfVisible(bool visible) {
  bool was_visible = self_visible_;
  self_visible_ = visible;
  if (was_visible != visible)
    SelfVisibleChanged();
}

void EmbeddedContentView::SetParentVisible(bool visible) {
  bool was_visible = parent_visible_;
  parent_visible_ = visible;
  if (was_visible != visible)
    ParentVisibleChanged();
}

}  // namespace blink
