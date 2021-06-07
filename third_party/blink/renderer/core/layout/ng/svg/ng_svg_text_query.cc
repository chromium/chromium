// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/svg/ng_svg_text_query.h"

#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/svg/layout_ng_svg_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"

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

std::tuple<Vector<const NGFragmentItem*>, const NGFragmentItems*>
FragmentItemsInLogicalOrder(const LayoutObject& query_root) {
  Vector<const NGFragmentItem*> item_list;
  const NGFragmentItems* items = nullptr;
  if (query_root.IsNGSVGText()) {
    DCHECK_LE(To<LayoutBox>(query_root).PhysicalFragmentCount(), 1u);
    for (const auto& fragment : To<LayoutBox>(query_root).PhysicalFragments()) {
      if (!fragment.Items())
        continue;
      items = fragment.Items();
      for (const auto& item : fragment.Items()->Items()) {
        if (item.Type() == NGFragmentItem::kSvgText)
          item_list.push_back(&item);
      }
    }
  } else {
    DCHECK(query_root.IsInLayoutNGInlineFormattingContext());
    NGInlineCursor cursor;
    cursor.MoveToIncludingCulledInline(query_root);
    items = &cursor.Items();
    for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
      const NGFragmentItem& item = *cursor.CurrentItem();
      if (item.Type() == NGFragmentItem::kSvgText)
        item_list.push_back(&item);
    }
  }
  // Sort |item_list| in the logical order.
  std::sort(item_list.begin(), item_list.end(),
            [](const NGFragmentItem* a, const NGFragmentItem* b) {
              return a->StartOffset() < b->StartOffset();
            });
  return std::tie(item_list, items);
}

}  // namespace

unsigned NGSvgTextQuery::NumberOfCharacters() const {
  Vector<const NGFragmentItem*> item_list;
  const NGFragmentItems* items;
  std::tie(item_list, items) = FragmentItemsInLogicalOrder(query_root_);

  unsigned addressable_character_count = 0;
  for (const auto* item : item_list)
    addressable_character_count += CodePointLength(item->Text(*items));
  return addressable_character_count;
}

float NGSvgTextQuery::SubStringLength(unsigned start_index,
                                      unsigned length) const {
  Vector<const NGFragmentItem*> item_list;
  const NGFragmentItems* items;
  std::tie(item_list, items) = FragmentItemsInLogicalOrder(query_root_);

  float total_length = 0.0f;
  unsigned character_index = 0;
  for (const auto* item : item_list) {
    if (character_index >= start_index) {
      if (start_index + length <= character_index)
        break;
      float inline_size = item->IsHorizontal()
                              ? item->SvgFragmentData()->rect.Width()
                              : item->SvgFragmentData()->rect.Height();
      total_length +=
          inline_size /
          To<LayoutSVGInlineText>(item->GetLayoutObject())->ScalingFactor();
    }
    character_index += CodePointLength(item->Text(*items));
  }
  return total_length;
}

}  // namespace blink
