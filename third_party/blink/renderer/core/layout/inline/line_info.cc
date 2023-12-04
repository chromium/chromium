// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/line_info.h"

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/core/layout/inline/inline_break_token.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

void LineInfo::Reset() {
  items_data_ = nullptr;
  line_style_ = nullptr;
  results_.Shrink(0);

  bfc_offset_ = BfcOffset();

  break_token_ = nullptr;
  parallel_flow_break_tokens_.Shrink(0);

  block_in_inline_layout_result_ = nullptr;

  available_width_ = LayoutUnit();
  width_ = LayoutUnit();
  hang_width_ = LayoutUnit();
  text_indent_ = LayoutUnit();

  annotation_block_start_adjustment_ = LayoutUnit();
  initial_letter_box_block_start_adjustment_ = LayoutUnit();
  initial_letter_box_block_size_ = LayoutUnit();

  start_ = {0, 0};
  end_item_index_ = 0;
  end_offset_for_justify_ = 0;

  text_align_ = ETextAlign::kLeft;
  base_direction_ = TextDirection::kLtr;

  use_first_line_style_ = false;
  is_last_line_ = false;
  has_forced_break_ = false;
  is_empty_line_ = false;
  has_line_even_if_empty_ = false;
  is_block_in_inline_ = false;
  has_overflow_ = false;
  has_trailing_spaces_ = false;
  needs_accurate_end_position_ = false;
  is_ruby_base_ = false;
  is_ruby_text_ = false;
  may_have_text_combine_item_ = false;
  allow_hang_for_alignment_ = false;
}

void LineInfo::SetLineStyle(const InlineNode& node,
                            const InlineItemsData& items_data,
                            bool use_first_line_style) {
  use_first_line_style_ = use_first_line_style;
  items_data_ = &items_data;
  const LayoutBox* box = node.GetLayoutBox();
  line_style_ = box->Style(use_first_line_style_);
  needs_accurate_end_position_ = ComputeNeedsAccurateEndPosition();
  is_ruby_base_ = box->IsRubyBase();
  is_ruby_text_ = box->IsRubyText();

  // Reset block start offset related members.
  annotation_block_start_adjustment_ = LayoutUnit();
  initial_letter_box_block_start_adjustment_ = LayoutUnit();
  initial_letter_box_block_size_ = LayoutUnit();
}

ETextAlign LineInfo::GetTextAlign(bool is_last_line) const {
  // See LayoutRubyBase::TextAlignmentForLine().
  if (is_ruby_base_)
    return ETextAlign::kJustify;

  // See LayoutRubyText::TextAlignmentForLine().
  if (is_ruby_text_ && LineStyle().GetTextAlign() ==
                           ComputedStyleInitialValues::InitialTextAlign())
    return ETextAlign::kJustify;

  return LineStyle().GetTextAlign(is_last_line);
}

bool LineInfo::ComputeNeedsAccurateEndPosition() const {
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

InlineItemTextIndex LineInfo::End() const {
  return GetBreakToken() ? GetBreakToken()->Start() : ItemsData().End();
}

unsigned LineInfo::EndTextOffset() const {
  return GetBreakToken() ? GetBreakToken()->StartTextOffset()
                         : ItemsData().text_content.length();
}

unsigned LineInfo::InflowEndOffset() const {
  for (const auto& item_result : base::Reversed(Results())) {
    DCHECK(item_result.item);
    const InlineItem& item = *item_result.item;
    if (item.Type() == InlineItem::kText ||
        item.Type() == InlineItem::kControl ||
        item.Type() == InlineItem::kAtomicInline) {
      return item_result.EndOffset();
    }
  }
  return StartOffset();
}

bool LineInfo::ShouldHangTrailingSpaces() const {
  if (RuntimeEnabledFeatures::
          HangingWhitespaceDoesNotDependOnAlignmentEnabled()) {
    return true;
  }
  if (!HasTrailingSpaces()) {
    return false;
  }
  if (!line_style_->ShouldWrapLine()) {
    return false;
  }
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

bool LineInfo::IsHyphenated() const {
  for (const InlineItemResult& item_result : base::Reversed(Results())) {
    if (item_result.Length()) {
      return item_result.is_hyphenated;
    }
  }
  return false;
}

void LineInfo::UpdateTextAlign() {
  text_align_ = GetTextAlign(IsLastLine());

  if (RuntimeEnabledFeatures::
          HangingWhitespaceDoesNotDependOnAlignmentEnabled()) {
    allow_hang_for_alignment_ = true;

    if (HasTrailingSpaces()) {
      bool should_hang;
      bool hang_is_conditional = false;
      switch (line_style_->GetWhiteSpaceCollapse()) {
        case WhiteSpaceCollapse::kCollapse:
        case WhiteSpaceCollapse::kPreserveBreaks:
          should_hang = true;
          break;
        case WhiteSpaceCollapse::kPreserve:
          should_hang = line_style_->ShouldWrapLine();
          hang_is_conditional = HasForcedBreak() || IsLastLine();
          break;
        case WhiteSpaceCollapse::kBreakSpaces:
          should_hang = false;
      }

      if (should_hang) {
        hang_width_ = ComputeTrailingSpaceWidth(&end_offset_for_justify_);

        // Conditional hang: only the part of the trailing spaces that overflow
        // the line actually hang.
        // https://drafts.csswg.org/css-text-4/#conditionally-hang
        if (hang_is_conditional) {
          hang_width_ = std::min(hang_width_, Width() - available_width_)
                            .ClampNegativeToZero();
        }
        return;
      }
    }

    hang_width_ = LayoutUnit();
  } else {
    allow_hang_for_alignment_ = false;

    if (HasTrailingSpaces() && line_style_->ShouldWrapLine()) {
      if (ShouldHangTrailingSpaces()) {
        hang_width_ = ComputeTrailingSpaceWidth(&end_offset_for_justify_);
        allow_hang_for_alignment_ = true;
        return;
      }
      hang_width_ = ComputeTrailingSpaceWidth();
    }
  }

  if (text_align_ == ETextAlign::kJustify)
    end_offset_for_justify_ = InflowEndOffset();
}

LayoutUnit LineInfo::ComputeTrailingSpaceWidth(unsigned* end_offset_out) const {
  if (!has_trailing_spaces_) {
    if (end_offset_out)
      *end_offset_out = InflowEndOffset();
    return LayoutUnit();
  }

  LayoutUnit trailing_spaces_width;
  for (const auto& item_result : base::Reversed(Results())) {
    DCHECK(item_result.item);
    const InlineItem& item = *item_result.item;

    // If this item is opaque to whitespace collapsing, whitespace before this
    // item maybe collapsed. Keep looking for previous items.
    if (item.EndCollapseType() == InlineItem::kOpaqueToCollapsing) {
      continue;
    }
    // These items should be opaque-to-collapsing.
    DCHECK(item.Type() != InlineItem::kFloating &&
           item.Type() != InlineItem::kOutOfFlowPositioned &&
           item.Type() != InlineItem::kBidiControl);

    if (item.Type() == InlineItem::kControl ||
        item_result.has_only_trailing_spaces) {
      trailing_spaces_width += item_result.inline_size;
      continue;
    }

    // The last text item may contain trailing spaces if this is a last line,
    // has a forced break, or is 'white-space: pre'.
    unsigned end_offset = item_result.EndOffset();
    DCHECK(end_offset);
    if (item.Type() == InlineItem::kText) {
      if (!item_result.Length()) {
        continue;  // Skip empty items. See `LineBreaker::HandleEmptyText`.
      }
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
        if (IsRtl(BaseDirection()))
          trailing_spaces_width += end_position;
        else
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

LayoutUnit LineInfo::ComputeWidth() const {
  LayoutUnit inline_size = TextIndent();
  for (const InlineItemResult& item_result : Results()) {
    inline_size += item_result.inline_size;
  }

  return inline_size;
}

#if DCHECK_IS_ON()
float LineInfo::ComputeWidthInFloat() const {
  float inline_size = TextIndent();
  for (const InlineItemResult& item_result : Results()) {
    inline_size += item_result.inline_size.ToFloat();
  }

  return inline_size;
}
#endif

// Block start adjustment and annotation ovreflow
//
//  Ruby without initial letter[1][2]:
//                  RUBY = annotation overflow and block start adjustment
//          This is line has ruby.
//
//  Raise/Sunken[3]: initial_letter_block_start > 0
//   block start adjustment
//        ***** ^
//          *   | block start adjustment
//          *   V
//          *       RUBY = not annotation overflow
//          *   his line has ruby.
//
//   Drop + Ruby(over)[4]: initial_letter_block_start == 0
//                  RUBY = annotation overflow and block start adjustment
//        ***** his line has ruby.
//          *
//          *
//          *
//          *
//
//  Ruby(over) is taller than initial letter[5]:
//                  RUBY = annotation overflow
//        *****     RUBY ^
//          *       RUBY | block start adjustment
//          *       RUBY |
//          *       RUBY V
//          *    his line has ruby.
//
//  Ruby(under) and raise/Sunken[6]:
//        ***** ^
//          *   | block start adjustment
//          *   V
//          *   his line has under ruby.
//          *       RUBY
//
//  Ruby(under) and drop[7]:
//               his line has under ruby
//        ******     RUBY
//          *
//          *
//
// [1] fast/ruby/ruby-position-modern-japanese-fonts.html
// [2] https://wpt.live/css/css-ruby/line-spacing.html
// [3]
// https://wpt.live/css/css-inline/initial-letter/initial-letter-block-position-raise-over-ruby.html
// [4]
// https://wpt.live/css/css-inline/initial-letter/initial-letter-block-position-drop-over-ruby.html
// [5]
// https://wpt.live/css/css-inline/initial-letter/initial-letter-block-position-raise-over-ruby.html
// [6]
// https://wpt.live/css/css-inline/initial-letter/initial-letter-block-position-raise-under-ruby.html
// [7]
// https://wpt.live/css/css-inline/initial-letter/initial-letter-block-position-drop-under-ruby.html

LayoutUnit LineInfo::ComputeAnnotationBlockOffsetAdjustment() const {
  if (annotation_block_start_adjustment_ < 0) {
    // Test[1] or `ruby-position:under` reach here.
    // [1] https://wpt.live/css/css-ruby/line-spacing.html
    return annotation_block_start_adjustment_ +
           initial_letter_box_block_start_adjustment_;
  }
  // The raise/sunken initial letter may cover annotations[2].
  // [2]
  // https://wpt.live/css/css-inline/initial-letter/initial-letter-block-position-raise-over-ruby.html
  return std::max(annotation_block_start_adjustment_ -
                      initial_letter_box_block_start_adjustment_,
                  LayoutUnit());
}

LayoutUnit LineInfo::ComputeBlockStartAdjustment() const {
  if (annotation_block_start_adjustment_ < 0) {
    // Test[1] or `ruby-position:under` reaches here.
    // [1] https://wpt.live/css/css-ruby/line-spacing.html
    return annotation_block_start_adjustment_ +
           initial_letter_box_block_start_adjustment_;
  }
  // The raise/sunken initial letter may cover annotations[2].
  // [2]
  // https://wpt.live/css/css-initial-letter/initial-letter-block-position-raise-over-ruby.html
  return std::max(annotation_block_start_adjustment_,
                  initial_letter_box_block_start_adjustment_);
}

LayoutUnit LineInfo::ComputeInitialLetterBoxBlockStartAdjustment() const {
  if (!annotation_block_start_adjustment_)
    return LayoutUnit();
  if (annotation_block_start_adjustment_ < 0) {
    return std::min(initial_letter_box_block_start_adjustment_ +
                        annotation_block_start_adjustment_,
                    LayoutUnit());
  }
  return std::max(annotation_block_start_adjustment_ -
                      initial_letter_box_block_start_adjustment_,
                  LayoutUnit());
}

LayoutUnit LineInfo::ComputeTotalBlockSize(
    LayoutUnit line_height,
    LayoutUnit annotation_overflow_block_end) const {
  DCHECK_GE(annotation_overflow_block_end, LayoutUnit());
  const LayoutUnit line_height_with_annotation =
      line_height + annotation_block_start_adjustment_ +
      annotation_overflow_block_end;
  return std::max(initial_letter_box_block_size_, line_height_with_annotation);
}

std::ostream& operator<<(std::ostream& ostream, const LineInfo& line_info) {
  // Feel free to add more LineInfo members.
  ostream << "LineInfo available_width_=" << line_info.AvailableWidth()
          << " width_=" << line_info.Width() << " Results=[\n";
  for (const auto& result : line_info.Results()) {
    ostream << "\t" << result.item->ToString() << "\n";
  }
  return ostream << "]";
}

}  // namespace blink
