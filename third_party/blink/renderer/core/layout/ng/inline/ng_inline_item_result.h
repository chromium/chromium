// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEM_RESULT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEM_RESULT_H_

#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class NGInlineItem;
class NGInlineNode;

struct NGInlineItemsData;

// The result of measuring NGInlineItem.
//
// This is a transient context object only while building line boxes.
// Produced while determining the line break points, but these data are needed
// to create line boxes.
//
// NGLineBreaker produces, and NGInlineLayoutAlgorithm consumes.
struct CORE_EXPORT NGInlineItemResult {
  DISALLOW_NEW();

 public:
  const NGTextOffset& TextOffset() const { return text_offset; }
  unsigned StartOffset() const { return text_offset.start; }
  unsigned EndOffset() const { return text_offset.end; }
  unsigned Length() const { return text_offset.Length(); }

  LayoutUnit HyphenInlineSize() const {
    return hyphen_shape_result->SnappedWidth().ClampNegativeToZero();
  }

  void ClearHyphen() {
    hyphen_string = String();
    hyphen_shape_result = nullptr;
  }

  // The NGInlineItem and its index.
  const NGInlineItem* item;
  unsigned item_index;

  // The range of text content for this item.
  NGTextOffset text_offset;

  // Indicates the limits of the trailing space run.
  base::Optional<unsigned> non_hangable_run_end;

  // Inline size of this item.
  LayoutUnit inline_size;

  // Pending inline-end overhang amount for RubyRun.
  // This is committed if a following item meets conditions.
  LayoutUnit pending_end_overhang;

  // ShapeResult for text items. Maybe different from NGInlineItem if re-shape
  // is needed in the line breaker.
  scoped_refptr<const ShapeResultView> shape_result;

  // Hyphen character and its |ShapeResult| if this text is hyphenated.
  String hyphen_string;
  scoped_refptr<const ShapeResult> hyphen_shape_result;

  // NGLayoutResult for atomic inline items.
  scoped_refptr<const NGLayoutResult> layout_result;

  // NGPositionedFloat for floating inline items. Should only be present for
  // positioned floats (not unpositioned). It indicates where it was placed
  // within the BFC.
  base::Optional<NGPositionedFloat> positioned_float;

  // Margins, borders, and padding for open tags.
  // Margins are set for atomic inlines too.
  NGLineBoxStrut margins;
  NGLineBoxStrut borders;
  NGLineBoxStrut padding;

  // Has start/end edge for open/close tags.
  bool has_edge = false;

  // Inside of this may be breakable. False means there are no break
  // opportunities, or has CSS properties that prohibit breaking.
  // Used only during line breaking.
  bool may_break_inside = false;

  // Lines can break after this item. Set for all items.
  // Used only during line breaking.
  bool can_break_after = false;

  // True if this item contains only trailing spaces.
  // Trailing spaces are measured differently that they are split from other
  // text items.
  // Used only when 'white-space: pre-wrap', because collapsible spaces are
  // removed, and if 'pre', trailing spaces are not different from other
  // characters.
  bool has_only_trailing_spaces = false;

  // The previous value of |break_anywhere_if_overflow| in the
  // NGInlineItemResults list. Like |should_create_line_box|, this value is used
  // to rewind properly.
  bool break_anywhere_if_overflow = false;

  // We don't create "certain zero-height line boxes".
  // https://drafts.csswg.org/css2/visuren.html#phantom-line-box
  // Such line boxes do not prevent two margins being "adjoining", and thus
  // collapsing.
  // https://drafts.csswg.org/css2/box.html#collapsing-margins
  //
  // This field should be initialized to the previous value in the
  // NGInlineItemResults list. If line breaker rewinds NGInlineItemResults
  // list, we can still look at the last value in the list to determine if we
  // need a line box. E.g.
  // [float should_create_line_box: false], [text should_create_line_box: true]
  //
  // If "text" doesn't fit, and we rewind so that we only have "float", we can
  // correctly determine that we don't need a line box.
  bool should_create_line_box = false;

  // This field should be initialized and maintained like
  // |should_create_line_box|. It indicates if there are (at the current
  // position) any unpositioned floats.
  bool has_unpositioned_floats = false;

  NGInlineItemResult();
  NGInlineItemResult(const NGInlineItem*,
                     unsigned index,
                     const NGTextOffset& text_offset,
                     bool break_anywhere_if_overflow,
                     bool should_create_line_box,
                     bool has_unpositioned_floats);

#if DCHECK_IS_ON()
  void CheckConsistency(bool allow_null_shape_result = false) const;
#endif
};

// Represents a set of NGInlineItemResult that form a line box.
using NGInlineItemResults = Vector<NGInlineItemResult, 32>;

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

  // If the line is marked as empty, it means that there's no content that
  // requires it to be present at all, e.g. when there are only close tags with
  // no margin/border/padding.
  bool IsEmptyLine() const { return is_empty_line_; }
  void SetIsEmptyLine() { is_empty_line_ = true; }

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
  LayoutUnit WidthForAlignment() const { return width_ - hang_width_; }
  // Width that hangs over the end of the line; e.g., preserved trailing spaces.
  LayoutUnit HangWidth() const { return hang_width_; }
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

  LayoutUnit available_width_;
  LayoutUnit width_;
  LayoutUnit hang_width_;
  LayoutUnit text_indent_;

  unsigned start_offset_;
  unsigned end_item_index_;
  unsigned end_offset_for_justify_;

  ETextAlign text_align_ = ETextAlign::kLeft;
  TextDirection base_direction_ = TextDirection::kLtr;

  bool use_first_line_style_ = false;
  bool is_last_line_ = false;
  bool is_empty_line_ = false;
  bool has_overflow_ = false;
  bool has_trailing_spaces_ = false;
  bool needs_accurate_end_position_ = false;
  bool is_ruby_base_ = false;
  bool is_ruby_text_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEM_RESULT_H_
