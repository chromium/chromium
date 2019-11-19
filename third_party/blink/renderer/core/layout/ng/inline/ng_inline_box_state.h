// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_BOX_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_BOX_STATE_H_

#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_height_metrics.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/fonts/font_baseline.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class NGInlineItem;
struct NGInlineItemResult;
class ShapeResultView;

// Fragments that require the layout position/size of ancestor are packed in
// this struct.
struct NGPendingPositions {
  unsigned fragment_start;
  unsigned fragment_end;
  NGLineHeightMetrics metrics;
  EVerticalAlign vertical_align;
};

// Represents the current box while NGInlineLayoutAlgorithm performs layout.
// Used 1) to cache common values for a box, and 2) to layout children that
// require ancestor position or size.
// This is a transient object only while building line boxes in a block.
struct NGInlineBoxState {
  DISALLOW_NEW();

 public:
  unsigned fragment_start = 0;
  const NGInlineItem* item = nullptr;
  const ComputedStyle* style = nullptr;

  // The united metrics for the current box. This includes all objects in this
  // box, including descendants, and adjusted by placement properties such as
  // 'vertical-align'.
  NGLineHeightMetrics metrics;

  // The metrics of the font for this box. This includes leadings as specified
  // by the 'line-height' property.
  NGLineHeightMetrics text_metrics;

  // The distance between the text-top and the baseline for this box. The
  // text-top does not include leadings.
  LayoutUnit text_top;

  // The height of the text fragments.
  LayoutUnit text_height;

  // These values are to create a box fragment. Set only when needs_box_fragment
  // is set.
  bool has_start_edge = false;
  bool has_end_edge = false;
  LayoutUnit margin_inline_start;
  LayoutUnit margin_inline_end;
  NGLineBoxStrut borders;
  NGLineBoxStrut padding;

  Vector<NGPendingPositions> pending_descendants;
  bool include_used_fonts = false;
  bool needs_box_fragment = false;

  // True if this box has a metrics, including pending ones. Pending metrics
  // will be activated in |EndBoxState()|.
  bool HasMetrics() const {
    return !metrics.IsEmpty() || !pending_descendants.IsEmpty();
  }

  // Compute text metrics for a box. All text in a box share the same
  // metrics.
  // The computed metrics is included into the line height of the current box.
  void ComputeTextMetrics(const ComputedStyle& style,
                          FontBaseline baseline_type);
  void EnsureTextMetrics(const ComputedStyle&, FontBaseline);
  void ResetTextMetrics();

  void AccumulateUsedFonts(const ShapeResultView*, FontBaseline);

  // 'text-top' offset for 'vertical-align'.
  LayoutUnit TextTop(FontBaseline baseline_type) const;

  // Create a box fragment for this box.
  void SetNeedsBoxFragment();

  // Returns if the text style can be added without open-tag.
  // Text with different font or vertical-align needs to be wrapped with an
  // inline box.
  bool CanAddTextOfStyle(const ComputedStyle&) const;

#if DCHECK_IS_ON()
  void CheckSame(const NGInlineBoxState&) const;
#endif
};

// Represents the inline tree structure. This class provides:
// 1) Allow access to fragments belonging to the current box.
// 2) Performs layout when the positin/size of a box was computed.
// 3) Cache common values for a box.
class CORE_EXPORT NGInlineLayoutStateStack {
  STACK_ALLOCATED();

 public:
  // The box state for the line box.
  NGInlineBoxState& LineBoxState() { return stack_.front(); }

  void SetIsEmptyLine(bool is_empty_line) { is_empty_line_ = is_empty_line; }

  // Initialize the box state stack for a new line.
  // @return The initial box state for the line.
  NGInlineBoxState* OnBeginPlaceItems(const ComputedStyle&, FontBaseline, bool);

  // Push a box state stack.
  NGInlineBoxState* OnOpenTag(const NGInlineItem&,
                              const NGInlineItemResult&,
                              const NGLineBoxFragmentBuilder::ChildList&);
  NGInlineBoxState* OnOpenTag(const ComputedStyle&,
                              const NGLineBoxFragmentBuilder::ChildList&);

  // Pop a box state stack.
  NGInlineBoxState* OnCloseTag(NGLineBoxFragmentBuilder::ChildList*,
                               NGInlineBoxState*,
                               FontBaseline,
                               bool has_end_edge = true);

  // Compute all the pending positioning at the end of a line.
  void OnEndPlaceItems(NGLineBoxFragmentBuilder::ChildList*, FontBaseline);

  bool HasBoxFragments() const { return !box_data_list_.IsEmpty(); }

  // Notify when child is inserted at |index| to adjust child indexes.
  void ChildInserted(unsigned index);

  // This class keeps indexes to fragments in the line box, and that only
  // appending is allowed. Call this function to move all such data to the line
  // box, so that outside of this class can reorder fragments in the line box.
  void PrepareForReorder(NGLineBoxFragmentBuilder::ChildList*);

  // When reordering was complete, call this function to re-construct the box
  // data from the line box. Callers must call |PrepareForReorder()| before
  // reordering.
  void UpdateAfterReorder(NGLineBoxFragmentBuilder::ChildList*);

  // Update start/end of the first BoxData found at |index|.
  //
  // If inline fragmentation is found, a new BoxData is added.
  //
  // Returns the index to process next. It should be given to the next call to
  // this function.
  unsigned UpdateBoxDataFragmentRange(NGLineBoxFragmentBuilder::ChildList*,
                                      unsigned index);

  // Update edges of inline fragmented boxes.
  void UpdateFragmentedBoxDataEdges();

  // Compute inline positions of fragments and boxes.
  LayoutUnit ComputeInlinePositions(NGLineBoxFragmentBuilder::ChildList*);

  // Create box fragments. This function turns a flat list of children into
  // a box tree.
  void CreateBoxFragments(NGLineBoxFragmentBuilder::ChildList*);

#if DCHECK_IS_ON()
  void CheckSame(const NGInlineLayoutStateStack&) const;
#endif

 private:
  // End of a box state, either explicitly by close tag, or implicitly at the
  // end of a line.
  void EndBoxState(NGInlineBoxState*,
                   NGLineBoxFragmentBuilder::ChildList*,
                   FontBaseline);

  void AddBoxFragmentPlaceholder(NGInlineBoxState*,
                                 NGLineBoxFragmentBuilder::ChildList*,
                                 FontBaseline);

  enum PositionPending { kPositionNotPending, kPositionPending };

  // Compute vertical position for the 'vertical-align' property.
  // The timing to apply varies by values; some values apply at the layout of
  // the box was computed. Other values apply when the layout of the parent or
  // the line box was computed.
  // https://www.w3.org/TR/CSS22/visudet.html#propdef-vertical-align
  // https://www.w3.org/TR/css-inline-3/#propdef-vertical-align
  PositionPending ApplyBaselineShift(NGInlineBoxState*,
                                     NGLineBoxFragmentBuilder::ChildList*,
                                     FontBaseline);

  // Compute the metrics for when 'vertical-align' is 'top' and 'bottom' from
  // |pending_descendants|.
  NGLineHeightMetrics MetricsForTopAndBottomAlign(
      const NGInlineBoxState&,
      const NGLineBoxFragmentBuilder::ChildList&) const;

  // Data for a box fragment. See AddBoxFragmentPlaceholder().
  // This is a transient object only while building a line box.
  struct BoxData {
    BoxData(unsigned start,
            unsigned end,
            const NGInlineItem* item,
            LogicalSize size)
        : fragment_start(start), fragment_end(end), item(item), size(size) {}

    BoxData(const BoxData& other, unsigned start, unsigned end)
        : fragment_start(start),
          fragment_end(end),
          item(other.item),
          size(other.size),
          offset(other.offset) {}

    void SetFragmentRange(unsigned start_index, unsigned end_index) {
      fragment_start = start_index;
      fragment_end = end_index;
    }

    // The range of child fragments this box contains.
    unsigned fragment_start;
    unsigned fragment_end;

    const NGInlineItem* item;
    LogicalSize size;

    bool has_line_left_edge = false;
    bool has_line_right_edge = false;
    NGLineBoxStrut padding;
    // |CreateBoxFragment()| needs margin, border+padding, and the sum of them.
    LayoutUnit margin_line_left;
    LayoutUnit margin_line_right;
    LayoutUnit margin_border_padding_line_left;
    LayoutUnit margin_border_padding_line_right;

    LogicalOffset offset;
    unsigned parent_box_data_index = 0;
    unsigned fragmented_box_data_index = 0;

    void UpdateFragmentEdges(Vector<BoxData, 4>& list);

    scoped_refptr<const NGLayoutResult> CreateBoxFragment(
        NGLineBoxFragmentBuilder::ChildList*);
  };

  Vector<NGInlineBoxState, 4> stack_;
  Vector<BoxData, 4> box_data_list_;

  bool is_empty_line_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_BOX_STATE_H_
