// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_outline_utils.h"

#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

bool NGOutlineUtils::HasPaintedOutline(const ComputedStyle& style,
                                       const Node* node) {
  if (!style.HasOutline() || style.Visibility() != EVisibility::kVisible)
    return false;
  if (style.OutlineStyleIsAuto() &&
      !LayoutTheme::GetTheme().ShouldDrawDefaultFocusRing(node, style))
    return false;
  return true;
}

bool NGOutlineUtils::ShouldPaintOutline(
    const NGPhysicalBoxFragment& physical_fragment) {
  if (!physical_fragment.IsInlineBox())
    return true;

  // In order to compute united outlines, collect all rectangles of inline
  // fragments for |LayoutInline| if |this| is the first inline fragment.
  // Otherwise return none.
  const LayoutObject* layout_object = physical_fragment.GetLayoutObject();
  DCHECK(layout_object);
  DCHECK(layout_object->IsLayoutInline());
  NGInlineCursor cursor;
  cursor.MoveTo(*layout_object);
  DCHECK(cursor);
  if (cursor.Current().BoxFragment() == &physical_fragment)
    return true;
  if (!cursor.IsBlockFragmented())
    return false;

  // When |LayoutInline| is block fragmented, unite rectangles for each block
  // fragment. To do this, return |true| if |this| is the first inline fragment
  // of a block fragment.
  while (true) {
    wtf_size_t fragment_index = cursor.CurrentContainerFragmentIndex();
    cursor.MoveToNextForSameLayoutObject();
    DCHECK(cursor);
    if (cursor.Current().BoxFragment() == &physical_fragment)
      return fragment_index != cursor.CurrentContainerFragmentIndex();
  }
}

}  // namespace blink
