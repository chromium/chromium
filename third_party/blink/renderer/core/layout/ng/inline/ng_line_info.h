// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_INFO_H_

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_bfc_offset.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"

namespace blink {

class ComputedStyle;
class NGInlineNode;
struct NGInlineItemsData;

// Represents a line to build.
//
// This is a transient context object only while building line boxes.
//
// NGLineBreaker produces, and NGInlineLayoutAlgorithm consumes.
class CORE_EXPORT NGLineInfo {
  STACK_ALLOCATED();

 public:
  const NGInlineItemsData& ItemsData() const {
    DCHECK(items_data_);
    return *items_data_;
  }

  // The style to use for the line.
  const ComputedStyle& LineStyle() const {
    DCHECK(line_style_);
    return *line_style_;
  }
  void SetLineStyle(const NGInlineNode&,
                    const NGInlineItemsData&,
                    bool use_first_line_style);

  // Use ::first-line style if true.
  // https://drafts.csswg.org/css-pseudo/#selectordef-first-line
  // This is false for the "first formatted line" if '::first-line' rule is not
  // used in the document.
  // https://www.w3.org/TR/CSS22/selector.html#first-formatted-line
  bool UseFirstLineStyle() const { return use_first_line_style_; }

  // The last line of a block, or the line ends with a forced line break.
  // https://drafts.csswg.org/css-text-3/#propdef-text-align-last
  bool IsLastLine() const { return is_last_line_; }
  void SetIsLastLine(bool is_last_line) { is_last_line_ = is_last_line; }

  // Whether this line has ended with a forced break or not. Note, the forced
  // break item may not be the last item if trailing items are included, or even
  // does not exist if synthesized for block-in-inline.
  bool HasForcedBreak() const { return has_forced_break_; }
  void SetHasForcedBreak() { has_forced_break_ = true; }

  // If the line is marked as empty, it means that there's no content that
  // requires it to be present at all, e.g. when there are only close tags with
  // no margin/border/padding.
  bool IsEmptyLine() const { return is_empty_line_; }
  void SetIsEmptyLine() { is_empty_line_ = true; }

  // If this line is empty, but still should have height as editable.
  bool HasLineEvenIfEmpty() const { return has_line_even_if_empty_; }
  void SetHasLineEvenIfEmpty() { has_line_even_if_empty_ = true; }

  // Returns true if this line is a block-in-inline.
  bool IsBlockInInline() const { return is_block_in_inline_; }
  void SetIsBlockInInline() { is_block_in_inline_ = true; }
  const NGBlockBreakToken* BlockInInlineBreakToken() const {
    if (!block_in_inline_layout_result_)
      return nullptr;

    return To<NGBlockBreakToken>(
        block_in_inline_layout_result_->PhysicalFragment().BreakToken());
  }

  // NGInlineItemResults for this line.
  NGInlineItemResults* MutableResults() { return &results_; }
  const NGInlineItemResults& Results() const { return results_; }

  void SetTextIndent(LayoutUnit indent) { text_indent_ = indent; }
  LayoutUnit TextIndent() const { return text_indent_; }

  ETextAlign TextAlign() const { return text_align_; }
  // Update |TextAlign()| and related fields. This depends on |IsLastLine()| and
  // that must be called after |SetIsLastLine()|.
  void UpdateTextAlign();

  NGBfcOffset BfcOffset() const { return bfc_offset_; }
  LayoutUnit AvailableWidth() const { return available_width_; }

  // The width of this line. Includes trailing spaces if they were preserved.
  // Negative width created by negative 'text-indent' is clamped to zero.
  LayoutUnit Width() const { return width_.ClampNegativeToZero(); }
  // Same as |Width()| but returns negative value as is. Preserved trailing
  // spaces may or may not be included, depends on |ShouldHangTrailingSpaces()|.
  LayoutUnit WidthForAlignment() const {
    return width_ - HangWidthForAlignment();
  }
  // Width that hangs over the end of the line; e.g., preserved trailing spaces.
  LayoutUnit HangWidth() const { return hang_width_; }
  // Same as |HangWidth()| but it may be 0 depending on
  // |ShouldHangTrailingSpaces()|.
  LayoutUnit HangWidthForAlignment() const {
    return allow_hang_for_alignment_ ? hang_width_ : LayoutUnit();
  }
  // Compute |Width()| from |Results()|. Used during line breaking, before
  // |Width()| is set. After line breaking, this should match to |Width()|
  // without clamping.
  LayoutUnit ComputeWidth() const;

#if DCHECK_IS_ON()
  // Returns width in float. This function is used for avoiding |LayoutUnit|
  // saturated addition of items in line.
  float ComputeWidthInFloat() const;
#endif

  bool HasTrailingSpaces() const { return has_trailing_spaces_; }
  void SetHasTrailingSpaces() { has_trailing_spaces_ = true; }
  bool ShouldHangTrailingSpaces() const;

  // True if this line has overflow, excluding preserved trailing spaces.
  bool HasOverflow() const { return has_overflow_; }
  void SetHasOverflow(bool value = true) { has_overflow_ = value; }

  void SetBfcOffset(const NGBfcOffset& bfc_offset) { bfc_offset_ = bfc_offset; }
  void SetWidth(LayoutUnit available_width, LayoutUnit width) {
    available_width_ = available_width;
    width_ = width;
  }

  // Start text offset of this line.
  unsigned StartOffset() const { return start_offset_; }
  void SetStartOffset(unsigned offset) { start_offset_ = offset; }
  // End text offset of this line, excluding out-of-flow objects such as
  // floating or positioned.
  unsigned InflowEndOffset() const;
  // End text offset for `text-align: justify`. This excludes preserved trailing
  // spaces. Available only when |TextAlign()| is |kJustify|.
  unsigned EndOffsetForJustify() const {
    DCHECK_EQ(text_align_, ETextAlign::kJustify);
    return end_offset_for_justify_;
  }
  // End item index of this line.
  unsigned EndItemIndex() const { return end_item_index_; }
  void SetEndItemIndex(unsigned index) { end_item_index_ = index; }

  // The base direction of this line for the bidi algorithm.
  TextDirection BaseDirection() const { return base_direction_; }
  void SetBaseDirection(TextDirection direction) {
    base_direction_ = direction;
  }

  // Whether an accurate end position is needed, typically for end, center, and
  // justify alignment.
  bool NeedsAccurateEndPosition() const { return needs_accurate_end_position_; }

  // The block-in-inline layout result.
  const NGLayoutResult* BlockInInlineLayoutResult() const {
    return block_in_inline_layout_result_;
  }
  void SetBlockInInlineLayoutResult(const NGLayoutResult* layout_result) {
    block_in_inline_layout_result_ = std::move(layout_result);
  }

  // |MayHaveTextCombineItem()| is used for treating text-combine box as
  // ideographic character during "text-align:justify".
  bool MayHaveTextCombineItem() const { return may_have_text_combine_item_; }
  void SetHaveTextCombineItem() { may_have_text_combine_item_ = true; }

  // Returns annotation block start adjustment base on annotation and initial
  // letter.
  LayoutUnit ComputeAnnotationBlockOffsetAdjustment() const;

  // Returns block start adjustment for line base on annotation and initial
  // letter.
  LayoutUnit ComputeBlockStartAdjustment() const;

  // Returns block start adjustment for initial letter box base on annotation
  // and initial letter.
  LayoutUnit ComputeInitialLetterBoxBlockStartAdjustment() const;

  // Returns total block size of this line to check whether we should use next
  // layout opportunity or not base on `line_height`, annotation and initial
  // letter box.
  LayoutUnit ComputeTotalBlockSize(
      LayoutUnit line_height,
      LayoutUnit annotation_overflow_block_end) const;

  void SetAnnotationBlockStartAdjustment(LayoutUnit amount) {
    DCHECK(!IsEmptyLine());
    annotation_block_start_adjustment_ = amount;
  }

  void SetInitialLetterBlockStartAdjustment(LayoutUnit amount) {
    DCHECK_GE(amount, LayoutUnit());
    DCHECK(!IsEmptyLine());
    initial_letter_box_block_start_adjustment_ = amount;
  }

  void SetInitialLetterBoxBlockSize(LayoutUnit block_size) {
    DCHECK_GE(block_size, LayoutUnit());
    initial_letter_box_block_size_ = block_size;
  }

 private:
  ETextAlign GetTextAlign(bool is_last_line = false) const;
  bool ComputeNeedsAccurateEndPosition() const;

  // The width of preserved trailing spaces.
  LayoutUnit ComputeTrailingSpaceWidth(
      unsigned* end_offset_out = nullptr) const;

  const NGInlineItemsData* items_data_ = nullptr;
  const ComputedStyle* line_style_ = nullptr;
  NGInlineItemResults results_;

  NGBfcOffset bfc_offset_;

  const NGLayoutResult* block_in_inline_layout_result_ = nullptr;

  LayoutUnit available_width_;
  LayoutUnit width_;
  LayoutUnit hang_width_;
  LayoutUnit text_indent_;

  LayoutUnit annotation_block_start_adjustment_;
  LayoutUnit initial_letter_box_block_start_adjustment_;
  LayoutUnit initial_letter_box_block_size_;

  unsigned start_offset_;
  unsigned end_item_index_;
  unsigned end_offset_for_justify_;

  ETextAlign text_align_ = ETextAlign::kLeft;
  TextDirection base_direction_ = TextDirection::kLtr;

  bool use_first_line_style_ = false;
  bool is_last_line_ = false;
  bool has_forced_break_ = false;
  bool is_empty_line_ = false;
  bool has_line_even_if_empty_ = false;
  bool is_block_in_inline_ = false;
  bool has_overflow_ = false;
  bool has_trailing_spaces_ = false;
  bool needs_accurate_end_position_ = false;
  bool is_ruby_base_ = false;
  bool is_ruby_text_ = false;
  // Even if text combine item causes line break, this variable is not reset.
  // This variable is used to add spacing before/after text combine items if
  // "text-align: justify".
  // Note: To avoid scanning |NGInlineItemResults|, this variable is true
  // when |NGInlineItemResult| to |results_|.
  bool may_have_text_combine_item_ = false;
  bool allow_hang_for_alignment_ = false;
};

std::ostream& operator<<(std::ostream& ostream, const NGLineInfo& line_info);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_INFO_H_
