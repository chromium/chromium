// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/inline_item_result.h"

#include "third_party/blink/renderer/core/layout/inline/inline_item.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

InlineItemResult::InlineItemResult(const InlineItem* item,
                                   unsigned index,
                                   const TextOffsetRange& text_offset,
                                   bool break_anywhere_if_overflow,
                                   bool should_create_line_box,
                                   bool has_unpositioned_floats)
    : item(item),
      item_index(index),
      text_offset(text_offset),
      break_anywhere_if_overflow(break_anywhere_if_overflow),
      should_create_line_box(should_create_line_box),
      has_unpositioned_floats(has_unpositioned_floats) {}

void InlineItemResult::ShapeHyphen() {
  DCHECK(!hyphen);
  DCHECK(item);
  DCHECK(item->Style());
  hyphen.Shape(*item->Style());
}

#if DCHECK_IS_ON()
void InlineItemResult::CheckConsistency(bool allow_null_shape_result) const {
  DCHECK(item);
  text_offset.AssertValid();
  DCHECK_GE(text_offset.start, item->StartOffset());
  DCHECK_LE(text_offset.end, item->EndOffset());
  if (item->Type() == InlineItem::kText) {
    if (!Length()) {
      // Empty text item should not have a `shape_result`.
      DCHECK(!shape_result);
      return;
    }
    if (allow_null_shape_result && !shape_result)
      return;
    DCHECK(shape_result);
    DCHECK_EQ(Length(), shape_result->NumCharacters());
    DCHECK_EQ(StartOffset(), shape_result->StartIndex());
    DCHECK_EQ(EndOffset(), shape_result->EndIndex());
  }
}
#endif

void InlineItemResult::Trace(Visitor* visitor) const {
  visitor->Trace(layout_result);
  if (positioned_float)
    visitor->Trace(positioned_float.value());
}

}  // namespace blink
