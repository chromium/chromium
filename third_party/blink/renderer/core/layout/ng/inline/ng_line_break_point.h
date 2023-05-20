// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_BREAK_POINT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_BREAK_POINT_H_

#include "base/check_op.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_text_index.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

//
// Represents a determined break point.
//
struct CORE_EXPORT NGLineBreakPoint {
  DISALLOW_NEW();

 public:
  NGLineBreakPoint() = default;
  NGLineBreakPoint(const NGInlineItemTextIndex& offset,
                   const NGInlineItemTextIndex& end,
                   bool is_hyphenated = false)
      : offset(offset), end(end), is_hyphenated(is_hyphenated) {}
  explicit NGLineBreakPoint(const NGInlineItemTextIndex& offset,
                            bool is_hyphenated = false)
      : NGLineBreakPoint(offset, offset, is_hyphenated) {}

  explicit operator bool() const { return offset.text_offset; }

  bool operator==(const NGLineBreakPoint& other) const {
    return offset == other.offset && end == other.end &&
           is_hyphenated == other.is_hyphenated;
  }

  // The line breaks before `offset`. The `offset` is also the start of the next
  // line, includes trailing spaces, while `end` doesn't.
  //
  // In the following example, the line should break before `next`:
  // ```
  // end <span> </span> next
  // ```
  // Then `offset` is at `n`, while `end` is at the next space of `end`.
  NGInlineItemTextIndex offset;
  NGInlineItemTextIndex end;

  // True when this break point has a hyphen.
  bool is_hyphenated = false;

#if EXPENSIVE_DCHECKS_ARE_ON()
  LayoutUnit line_width;
#endif  // EXPENSIVE_DCHECKS_ARE_ON()
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_BREAK_POINT_H_
