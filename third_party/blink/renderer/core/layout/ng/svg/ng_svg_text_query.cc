// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/svg/ng_svg_text_query.h"

#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/svg/layout_ng_svg_text.h"

namespace blink {

namespace {

unsigned NextCodePointOffset(StringView string, unsigned offset) {
  ++offset;
  if (offset < string.length() && U16_IS_LEAD(string[offset - 1]) &&
      U16_IS_TRAIL(string[offset]))
    ++offset;
  return offset;
}

unsigned CodePointLength(StringView string) {
  unsigned count = 0;
  for (unsigned text_offset = 0; text_offset < string.length();
       text_offset = NextCodePointOffset(string, text_offset)) {
    ++count;
  }
  return count;
}

}  // namespace

unsigned NGSvgTextQuery::NumberOfCharacters() const {
  if (query_root_.IsNGSVGText()) {
    unsigned addressable_character_count = 0;
    DCHECK_LE(To<LayoutBox>(query_root_).PhysicalFragmentCount(), 1u);
    for (const auto& fragment :
         To<LayoutBox>(query_root_).PhysicalFragments()) {
      if (!fragment.Items())
        continue;
      for (const auto& item : fragment.Items()->Items()) {
        if (item.Type() != NGFragmentItem::kSvgText)
          continue;
        addressable_character_count +=
            CodePointLength(item.Text(*fragment.Items()));
      }
    }
    return addressable_character_count;
  }

  DCHECK(query_root_.IsInLayoutNGInlineFormattingContext());
  unsigned addressable_character_count = 0;
  NGInlineCursor cursor;
  for (cursor.MoveToIncludingCulledInline(query_root_); cursor;
       cursor.MoveToNextForSameLayoutObject()) {
    const NGFragmentItem& item = *cursor.CurrentItem();
    if (item.Type() != NGFragmentItem::kSvgText)
      continue;
    addressable_character_count += CodePointLength(cursor.CurrentText());
  }
  return addressable_character_count;
}

}  // namespace blink
