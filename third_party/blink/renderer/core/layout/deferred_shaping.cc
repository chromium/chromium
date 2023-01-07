// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/deferred_shaping.h"

namespace blink {

DeferredShapingViewportScope::DeferredShapingViewportScope(
    const LayoutView& layout_view)
    : ds_controller_(layout_view.GetDeferredShapingController()),
      previous_value_(ds_controller_.CurrentViewportBottom()) {
  const auto* scrollable_area = layout_view.GetScrollableArea();
  LayoutUnit viewport_top =
      LayoutUnit(scrollable_area ? scrollable_area->GetScrollOffset().y() : 0);
  LayoutUnit viewport_height =
      layout_view.InitialContainingBlockSize().block_size;
  ds_controller_.SetCurrentViewportBottom(PassKey(),
                                          viewport_top + viewport_height);
}

}  // namespace blink
