// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEM_RESULT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEM_RESULT_H_

#include "base/dcheck_is_on.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_text_offset.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class NGInlineItem;
class NGLayoutResult;
class ShapeResult;
class ShapeResultView;
struct NGPositionedFloat;

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

  // Compute/clear |hyphen_string| and |hyphen_shape_result|.
  void ShapeHyphen();
  void ClearHyphen() {
    hyphen_string = String();
    hyphen_shape_result = nullptr;
  }

  void Trace(Visitor* visitor) const;

  // The NGInlineItem and its index.
  const NGInlineItem* item;
  unsigned item_index;

  // The range of text content for this item.
  NGTextOffset text_offset;

  // Indicates the limits of the trailing space run.
  absl::optional<unsigned> non_hangable_run_end;

  // Inline size of this item.
  LayoutUnit inline_size;

  // Non-zero if text-combine after non-ideographic character
  // See "text-combine-justify.html".
  LayoutUnit spacing_before;

  // Pending inline-end overhang amount for RubyRun.
  // This is committed if a following item meets conditions.
  LayoutUnit pending_end_overhang;

  // ShapeResult for text items. Maybe different from NGInlineItem if re-shape
  // is needed in the line breaker.
  scoped_refptr<const ShapeResultView> shape_result;

  // Hyphen character and its |ShapeResult|.
  // Use |is_hyphenated| to determine whether this item is hyphenated or not.
  // These fields may be set even when this item is not hyphenated.
  String hyphen_string;
  scoped_refptr<const ShapeResult> hyphen_shape_result;

  // NGLayoutResult for atomic inline items.
  Member<const NGLayoutResult> layout_result;

  // NGPositionedFloat for floating inline items. Should only be present for
  // positioned floats (not unpositioned). It indicates where it was placed
  // within the BFC.
  GC_PLUGIN_IGNORE("crbug.com/1146383")
  absl::optional<NGPositionedFloat> positioned_float;

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

  // True if this is hyphenated. The hyphen is in |hyphen_string| and
  // |hyphen_shape_result|.
  bool is_hyphenated = false;

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
using NGInlineItemResults = HeapVector<NGInlineItemResult, 32>;

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::NGInlineItemResult)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEM_RESULT_H_
