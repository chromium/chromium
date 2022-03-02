// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/deferred_shaping.h"

#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"

namespace blink {

DeferredShapingViewportScope::DeferredShapingViewportScope(
    LocalFrameView& view,
    const LayoutView& layout_view)
    : view_(view), previous_value_(view.CurrentViewportBottom()) {
  LayoutUnit viewport_top =
      LayoutUnit(layout_view.GetScrollableArea()
                     ? view.GetScrollableArea()->GetScrollOffset().y()
                     : 0);
  LayoutUnit viewport_height =
      layout_view.InitialContainingBlockSize().block_size;
  view_.SetCurrentViewportBottom(
      PassKey(),
      viewport_top + viewport_height +
          LayoutUnit(viewport_height *
                     DisplayLockDocumentState::kViewportMarginPercentage /
                     100));
}

}  // namespace blink
