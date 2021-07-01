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
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

namespace {

unsigned CodePointLength(StringView string) {
  unsigned count = 0;
  for (unsigned text_offset = 0; text_offset < string.length();
       text_offset = string.NextCodePointOffset(text_offset)) {
    ++count;
  }
  return count;
}

std::tuple<Vector<const NGFragmentItem*>, const NGFragmentItems*>
FragmentItemsInVisualOrder(const LayoutObject& query_root) {
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
  return std::tie(item_list, items);
}

std::tuple<Vector<const NGFragmentItem*>, const NGFragmentItems*>
FragmentItemsInLogicalOrder(const LayoutObject& query_root) {
  auto items_tuple = FragmentItemsInVisualOrder(query_root);
  auto& item_list = std::get<0>(items_tuple);
  // Sort |item_list| in the logical order.
  std::sort(item_list.begin(), item_list.end(),
            [](const NGFragmentItem* a, const NGFragmentItem* b) {
              return a->StartOffset() < b->StartOffset();
            });
  return items_tuple;
}

const NGFragmentItem* FindFragmentItemForAddressableCharacterIndex(
    const LayoutObject& query_root,
    unsigned index) {
  Vector<const NGFragmentItem*> item_list;
  const NGFragmentItems* items;
  std::tie(item_list, items) = FragmentItemsInLogicalOrder(query_root);

  unsigned character_index = 0;
  for (const auto* item : item_list) {
    unsigned item_length = CodePointLength(item->Text(*items));
    if (character_index <= index && index < character_index + item_length)
      return item;
    character_index += item_length;
  }
  return nullptr;
}

void GetCanvasRotation(void* context,
                       unsigned,
                       Glyph,
                       FloatSize,
                       float,
                       bool,
                       CanvasRotationInVertical rotation,
                       const SimpleFontData*) {
  auto* canvas_rotation = static_cast<CanvasRotationInVertical*>(context);
  *canvas_rotation = rotation;
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
      total_length += inline_size / item->SvgScalingFactor();
    }
    character_index += CodePointLength(item->Text(*items));
  }
  return total_length;
}

FloatPoint NGSvgTextQuery::StartPositionOfCharacter(unsigned index) const {
  const NGFragmentItem* item =
      FindFragmentItemForAddressableCharacterIndex(query_root_, index);
  DCHECK(item);
  DCHECK_EQ(item->Type(), NGFragmentItem::kSvgText);
  if (item->IsHiddenForPaint())
    return FloatPoint();
  const auto& inline_text = *To<LayoutSVGInlineText>(item->GetLayoutObject());
  const float ascent =
      inline_text.ScaledFont().PrimaryFont()->GetFontMetrics().FloatAscent(
          item->Style().GetFontBaseline());
  const auto& item_rect = item->SvgFragmentData()->rect;
  const bool is_ltr = IsLtr(item->ResolvedDirection());
  FloatPoint point;
  if (item->IsHorizontal()) {
    point = is_ltr ? item_rect.Location() : item_rect.MaxXMinYCorner();
    point.Move(0.0f, ascent);
  } else {
    point = is_ltr ? item_rect.MaxXMinYCorner() : item_rect.MaxXMaxYCorner();
    point.Move(-ascent, 0.0f);
  }
  if (item->HasSvgTransformForBoundingBox())
    point = item->BuildSvgTransformForBoundingBox().MapPoint(point);
  const float scaling_factor = inline_text.ScalingFactor();
  point.Scale(1 / scaling_factor, 1 / scaling_factor);
  return point;
}

FloatPoint NGSvgTextQuery::EndPositionOfCharacter(unsigned index) const {
  const NGFragmentItem* item =
      FindFragmentItemForAddressableCharacterIndex(query_root_, index);
  DCHECK(item);
  DCHECK_EQ(item->Type(), NGFragmentItem::kSvgText);
  if (item->IsHiddenForPaint())
    return FloatPoint();
  const auto& inline_text = *To<LayoutSVGInlineText>(item->GetLayoutObject());
  const float ascent =
      inline_text.ScaledFont().PrimaryFont()->GetFontMetrics().FloatAscent(
          item->Style().GetFontBaseline());
  const auto& item_rect = item->SvgFragmentData()->rect;
  const bool is_ltr = IsLtr(item->ResolvedDirection());
  FloatPoint point;
  if (item->IsHorizontal()) {
    point = is_ltr ? item_rect.MaxXMinYCorner() : item_rect.Location();
    point.Move(0.0f, ascent);
  } else {
    point = is_ltr ? item_rect.MaxXMaxYCorner() : item_rect.MaxXMinYCorner();
    point.Move(-ascent, 0.0f);
  }
  if (item->HasSvgTransformForBoundingBox())
    point = item->BuildSvgTransformForBoundingBox().MapPoint(point);
  const float scaling_factor = inline_text.ScalingFactor();
  point.Scale(1 / scaling_factor, 1 / scaling_factor);
  return point;
}

FloatRect NGSvgTextQuery::ExtentOfCharacter(unsigned index) const {
  const NGFragmentItem* item =
      FindFragmentItemForAddressableCharacterIndex(query_root_, index);
  DCHECK(item);
  DCHECK_EQ(item->Type(), NGFragmentItem::kSvgText);
  if (item->IsHiddenForPaint())
    return FloatRect();
  return item->ObjectBoundingBox();
}

float NGSvgTextQuery::RotationOfCharacter(unsigned index) const {
  const NGFragmentItem* item =
      FindFragmentItemForAddressableCharacterIndex(query_root_, index);
  DCHECK(item);
  DCHECK_EQ(item->Type(), NGFragmentItem::kSvgText);
  if (item->IsHiddenForPaint())
    return 0.0f;
  float rotation = item->SvgFragmentData()->angle;
  if (item->Style().IsHorizontalWritingMode())
    return rotation;
  ETextOrientation orientation = item->Style().GetTextOrientation();
  if (orientation == ETextOrientation::kUpright)
    return rotation;
  if (orientation == ETextOrientation::kSideways)
    return rotation + 90.0f;
  DCHECK_EQ(orientation, ETextOrientation::kMixed);
  CanvasRotationInVertical canvas_rotation;
  // GetCanvasRotation() is usually called only once because a single
  // NGFragmentItem represents a single glyph in SVG <text>.
  item->TextShapeResult()->ForEachGlyph(0, GetCanvasRotation, &canvas_rotation);
  if (IsCanvasRotationInVerticalUpright(canvas_rotation))
    return rotation;
  return rotation + 90.0f;
}

// https://svgwg.org/svg2-draft/text.html#__svg__SVGTextContentElement__getCharNumAtPosition
int NGSvgTextQuery::CharacterNumberAtPosition(
    const FloatPoint& position) const {
  // The specification says we should do hit-testing in logical order.
  // However, this does it in visual order in order to match to the legacy SVG
  // <text> behavior.
  Vector<const NGFragmentItem*> item_list;
  const NGFragmentItems* items;
  std::tie(item_list, items) = FragmentItemsInVisualOrder(query_root_);

  const NGFragmentItem* hit_item = nullptr;
  for (const auto* item : item_list) {
    if (!item->IsHiddenForPaint() && item->Contains(position)) {
      hit_item = item;
      break;
    }
  }
  if (!hit_item)
    return -1;

  // Count code points before |hit_item|.
  std::sort(item_list.begin(), item_list.end(),
            [](const NGFragmentItem* a, const NGFragmentItem* b) {
              return a->StartOffset() < b->StartOffset();
            });
  unsigned addressable_character_count = 0;
  for (const auto* item : item_list) {
    if (item == hit_item)
      break;
    addressable_character_count += CodePointLength(item->Text(*items));
  }
  return addressable_character_count;
}

}  // namespace blink
