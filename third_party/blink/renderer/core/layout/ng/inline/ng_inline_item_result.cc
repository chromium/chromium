// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_result.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

NGInlineItemResult::NGInlineItemResult() : item(nullptr), item_index(0) {}

NGInlineItemResult::NGInlineItemResult(const NGInlineItem* item,
                                       unsigned index,
                                       const NGTextOffset& text_offset,
                                       bool break_anywhere_if_overflow,
                                       bool should_create_line_box,
                                       bool has_unpositioned_floats)
    : item(item),
      item_index(index),
      text_offset(text_offset),
      break_anywhere_if_overflow(break_anywhere_if_overflow),
      should_create_line_box(should_create_line_box),
      has_unpositioned_floats(has_unpositioned_floats) {}

void NGLineInfo::SetLineStyle(const NGInlineNode& node,
                              const NGInlineItemsData& items_data,
                              bool use_first_line_style) {
  use_first_line_style_ = use_first_line_style;
  items_data_ = &items_data;
  const LayoutBox* box = node.GetLayoutBox();
  line_style_ = box->Style(use_first_line_style_);
  needs_accurate_end_position_ = ComputeNeedsAccurateEndPosition();
  is_ruby_base_ = box->IsRubyBase();
  is_ruby_text_ = box->IsRubyText();
}

ETextAlign NGLineInfo::GetTextAlign(bool is_last_line) const {
  // See LayoutRubyBase::TextAlignmentForLine().
  if (is_ruby_base_)
    return ETextAlign::kJustify;

  // See LayoutRubyText::TextAlignmentForLine().
  if (is_ruby_text_ && LineStyle().GetTextAlign() ==
                           ComputedStyleInitialValues::InitialTextAlign())
    return ETextAlign::kJustify;

  return LineStyle().GetTextAlign(is_last_line);
}

bool NGLineInfo::ComputeNeedsAccurateEndPosition() const {
  // Some 'text-align' values need accurate end position. At this point, we
  // don't know if this is the last line or not, and thus we don't know whether
  // 'text-align' is used or 'text-align-last' is used.
  switch (GetTextAlign()) {
    case ETextAlign::kStart:
      break;
    case ETextAlign::kEnd:
    case ETextAlign::kCenter:
    case ETextAlign::kWebkitCenter:
    case ETextAlign::kJustify:
      return true;
    case ETextAlign::kLeft:
    case ETextAlign::kWebkitLeft:
      if (IsRtl(BaseDirection()))
        return true;
      break;
    case ETextAlign::kRight:
    case ETextAlign::kWebkitRight:
      if (IsLtr(BaseDirection()))
        return true;
      break;
  }
  ETextAlignLast align_last = LineStyle().TextAlignLast();
  if (is_ruby_base_) {
    // See LayoutRubyBase::TextAlignmentForLine().
    align_last = ETextAlignLast::kJustify;
  } else if (is_ruby_text_ &&
             align_last == ComputedStyleInitialValues::InitialTextAlignLast()) {
    // See LayoutRubyText::TextAlignmentForLine().
    align_last = ETextAlignLast::kJustify;
  }
  switch (align_last) {
    case ETextAlignLast::kStart:
    case ETextAlignLast::kAuto:
      return false;
    case ETextAlignLast::kEnd:
    case ETextAlignLast::kCenter:
    case ETextAlignLast::kJustify:
      return true;
    case ETextAlignLast::kLeft:
      if (IsRtl(BaseDirection()))
        return true;
      break;
    case ETextAlignLast::kRight:
      if (IsLtr(BaseDirection()))
        return true;
      break;
  }
  return false;
}

#if DCHECK_IS_ON()
void NGInlineItemResult::CheckConsistency(bool allow_null_shape_result) const {
  DCHECK(item);
  if (item->Type() == NGInlineItem::kText) {
    text_offset.AssertNotEmpty();
    if (allow_null_shape_result && !shape_result)
      return;
    DCHECK(shape_result);
    DCHECK_EQ(Length(), shape_result->NumCharacters());
    DCHECK_EQ(StartOffset(), shape_result->StartIndex());
    DCHECK_EQ(EndOffset(), shape_result->EndIndex());
  }
}
#endif

unsigned NGLineInfo::InflowEndOffset() const {
  const NGInlineItemResults& item_results = Results();
  for (auto it = item_results.rbegin(); it != item_results.rend(); ++it) {
    const NGInlineItemResult& item_result = *it;
    DCHECK(item_result.item);
    const NGInlineItem& item = *item_result.item;
    if (item.Type() == NGInlineItem::kText ||
        item.Type() == NGInlineItem::kControl ||
        item.Type() == NGInlineItem::kAtomicInline)
      return item_result.EndOffset();
  }
  return StartOffset();
}

bool NGLineInfo::ShouldHangTrailingSpaces() const {
  DCHECK(HasTrailingSpaces());
  if (!line_style_->AutoWrap())
    return false;
  switch (text_align_) {
    case ETextAlign::kStart:
    case ETextAlign::kJustify:
      return true;
    case ETextAlign::kEnd:
    case ETextAlign::kCenter:
    case ETextAlign::kWebkitCenter:
      return false;
    case ETextAlign::kLeft:
    case ETextAlign::kWebkitLeft:
      return IsLtr(BaseDirection());
    case ETextAlign::kRight:
    case ETextAlign::kWebkitRight:
      return IsRtl(BaseDirection());
  }
  NOTREACHED();
}

void NGLineInfo::UpdateTextAlign() {
  text_align_ = GetTextAlign(IsLastLine());

  if (HasTrailingSpaces() && ShouldHangTrailingSpaces()) {
    hang_width_ = ComputeTrailingSpaceWidth(&end_offset_for_justify_);
  } else if (text_align_ == ETextAlign::kJustify) {
    end_offset_for_justify_ = InflowEndOffset();
  }
}

LayoutUnit NGLineInfo::ComputeTrailingSpaceWidth(
    unsigned* end_offset_out) const {
  if (!has_trailing_spaces_) {
    if (end_offset_out)
      *end_offset_out = InflowEndOffset();
    return LayoutUnit();
  }

  const NGInlineItemResults& item_results = Results();
  LayoutUnit trailing_spaces_width;
  for (auto it = item_results.rbegin(); it != item_results.rend(); ++it) {
    const NGInlineItemResult& item_result = *it;
    DCHECK(item_result.item);
    const NGInlineItem& item = *item_result.item;

    // If this item is opaque to whitespace collapsing, whitespace before this
    // item maybe collapsed. Keep looking for previous items.
    if (item.EndCollapseType() == NGInlineItem::kOpaqueToCollapsing) {
      continue;
    }
    // These items should be opaque-to-collapsing.
    DCHECK(item.Type() != NGInlineItem::kFloating &&
           item.Type() != NGInlineItem::kOutOfFlowPositioned &&
           item.Type() != NGInlineItem::kBidiControl);

    if (item.Type() == NGInlineItem::kControl ||
        item_result.has_only_trailing_spaces) {
      trailing_spaces_width += item_result.inline_size;
      continue;
    }

    // The last text item may contain trailing spaces if this is a last line,
    // has a forced break, or is 'white-space: pre'.
    unsigned end_offset = item_result.EndOffset();
    DCHECK(end_offset);
    if (item.Type() == NGInlineItem::kText) {
      const String& text = items_data_->text_content;
      if (end_offset && text[end_offset - 1] == kSpaceCharacter) {
        do {
          --end_offset;
        } while (end_offset > item_result.StartOffset() &&
                 text[end_offset - 1] == kSpaceCharacter);

        // If all characters in this item_result are spaces, check next item.
        if (end_offset == item_result.StartOffset()) {
          trailing_spaces_width += item_result.inline_size;
          continue;
        }

        // To compute the accurate width, we need to reshape if |end_offset| is
        // not safe-to-break. We avoid reshaping in this case because the cost
        // is high and the difference is subtle for the purpose of this
        // function.
        // TODO(kojii): This does not compute correctly for RTL. Need to re-work
        // when we support UAX#9 L1.
        // TODO(kojii): Compute this without |CreateShapeResult|.
        scoped_refptr<ShapeResult> shape_result =
            item_result.shape_result->CreateShapeResult();
        float end_position = shape_result->PositionForOffset(
            end_offset - shape_result->StartIndex());
        trailing_spaces_width += shape_result->Width() - end_position;
      }
    }

    if (end_offset_out)
      *end_offset_out = end_offset;
    return trailing_spaces_width;
  }

  // An empty line, or only trailing spaces.
  if (end_offset_out)
    *end_offset_out = StartOffset();
  return trailing_spaces_width;
}

LayoutUnit NGLineInfo::ComputeWidth() const {
  LayoutUnit inline_size = TextIndent();
  for (const NGInlineItemResult& item_result : Results())
    inline_size += item_result.inline_size;

  return inline_size;
}

#if DCHECK_IS_ON()
float NGLineInfo::ComputeWidthInFloat() const {
  float inline_size = TextIndent();
  for (const NGInlineItemResult& item_result : Results())
    inline_size += item_result.inline_size.ToFloat();

  return inline_size;
}
#endif

std::ostream& operator<<(std::ostream& ostream, const NGLineInfo& line_info) {
  // Feel free to add more NGLneInfo members.
  ostream << "NGLineInfo available_width_=" << line_info.AvailableWidth()
          << " width_=" << line_info.Width() << " Results=[\n";
  for (const auto& result : line_info.Results()) {
    ostream << "\t" << result.item->ToString() << "\n";
  }
  return ostream << "]";
}

}  // namespace blink
