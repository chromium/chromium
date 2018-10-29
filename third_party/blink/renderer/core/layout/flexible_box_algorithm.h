/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEXIBLE_BOX_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEXIBLE_BOX_ALGORITHM_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/min_max_size.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/order_iterator.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class FlexItem;
class FlexLayoutAlgorithm;
class LayoutBox;
struct MinMaxSize;

enum FlexSign {
  kPositiveFlexibility,
  kNegativeFlexibility,
};

enum class TransformedWritingMode {
  kTopToBottomWritingMode,
  kRightToLeftWritingMode,
  kLeftToRightWritingMode,
  kBottomToTopWritingMode
};

typedef Vector<FlexItem, 8> FlexItemVector;

class FlexItem {
 public:
  // flex_base_content_size includes scrollbar width but not border or padding.
  FlexItem(LayoutBox*,
           LayoutUnit flex_base_content_size,
           MinMaxSize min_max_sizes,
           LayoutUnit main_axis_border_and_padding,
           LayoutUnit main_axis_margin);

  LayoutUnit HypotheticalMainAxisMarginBoxSize() const {
    return hypothetical_main_content_size + main_axis_border_and_padding +
           main_axis_margin;
  }

  LayoutUnit FlexBaseMarginBoxSize() const {
    return flex_base_content_size + main_axis_border_and_padding +
           main_axis_margin;
  }

  LayoutUnit FlexedBorderBoxSize() const {
    return flexed_content_size + main_axis_border_and_padding;
  }

  LayoutUnit FlexedMarginBoxSize() const {
    return flexed_content_size + main_axis_border_and_padding +
           main_axis_margin;
  }

  LayoutUnit ClampSizeToMinAndMax(LayoutUnit size) const {
    return min_max_sizes.ClampSizeToMinAndMax(size);
  }

  ItemPosition Alignment() const;

  bool HasOrthogonalFlow() const;

  LayoutUnit FlowAwareMarginStart() const;
  LayoutUnit FlowAwareMarginEnd() const;
  LayoutUnit FlowAwareMarginBefore() const;
  LayoutUnit CrossAxisMarginExtent() const;

  LayoutUnit MarginBoxAscent() const;

  LayoutUnit AvailableAlignmentSpace(LayoutUnit) const;

  bool HasAutoMarginsInCrossAxis() const;

  void UpdateAutoMarginsInMainAxis(LayoutUnit auto_margin_offset);

  FlexLayoutAlgorithm* algorithm;
  LayoutBox* box;
  const LayoutUnit flex_base_content_size;
  const MinMaxSize min_max_sizes;
  const LayoutUnit hypothetical_main_content_size;
  const LayoutUnit main_axis_border_and_padding;
  const LayoutUnit main_axis_margin;
  LayoutUnit flexed_content_size;

  LayoutUnit cross_axis_size;
  LayoutUnit cross_axis_intrinsic_size;
  LayoutPoint desired_location;

  bool frozen;

  // TODO(dgrogan): Change this to NGBlockNode when all items are blockified.
  NGLayoutInputNode ng_input_node;
  scoped_refptr<NGLayoutResult> layout_result;
};

class FlexItemVectorView {
 public:
  FlexItemVectorView(FlexItemVector* flex_vector,
                     wtf_size_t start,
                     wtf_size_t end)
      : vector_(flex_vector), start_(start), end_(end) {
    DCHECK_LT(start_, end_);
    DCHECK_LE(end_, vector_->size());
  }

  wtf_size_t size() const { return end_ - start_; }
  FlexItem& operator[](wtf_size_t i) { return vector_->at(start_ + i); }
  const FlexItem& operator[](wtf_size_t i) const {
    return vector_->at(start_ + i);
  }

 private:
  FlexItemVector* vector_;
  wtf_size_t start_;
  wtf_size_t end_;
};

class FlexLine {
 public:
  typedef Vector<FlexItem*, 8> ViolationsVector;

  // This will std::move the passed-in line_items.
  FlexLine(FlexLayoutAlgorithm* algorithm,
           FlexItemVectorView line_items,
           LayoutUnit container_logical_width,
           LayoutUnit sum_flex_base_size,
           double total_flex_grow,
           double total_flex_shrink,
           double total_weighted_flex_shrink,
           LayoutUnit sum_hypothetical_main_size)
      : algorithm(algorithm),
        line_items(std::move(line_items)),
        container_logical_width(container_logical_width),
        sum_flex_base_size(sum_flex_base_size),
        total_flex_grow(total_flex_grow),
        total_flex_shrink(total_flex_shrink),
        total_weighted_flex_shrink(total_weighted_flex_shrink),
        sum_hypothetical_main_size(sum_hypothetical_main_size) {}

  FlexSign Sign() const {
    return sum_hypothetical_main_size < container_main_inner_size
               ? kPositiveFlexibility
               : kNegativeFlexibility;
  }

  void SetContainerMainInnerSize(LayoutUnit size) {
    container_main_inner_size = size;
  }

  void FreezeInflexibleItems();

  // This modifies remaining_free_space.
  void FreezeViolations(ViolationsVector& violations);

  // Should be called in a loop until it returns false.
  // This modifies remaining_free_space.
  bool ResolveFlexibleLengths();

  // Distributes remaining_free_space across the main axis auto margins
  // of the flex items of this line and returns the amount that should be
  // used for each auto margins. If there are no auto margins, leaves
  // remaining_free_space unchanged.
  LayoutUnit ApplyMainAxisAutoMarginAdjustment();

  // Computes & sets desired_position on the FlexItems on this line.
  // Before calling this function, the items need to be laid out with
  // flexed_content_size set as the override main axis size, and
  // cross_axis_size needs to be set correctly on each flex item (to the size
  // the item has without stretching).
  void ComputeLineItemsPosition(LayoutUnit main_axis_offset,
                                LayoutUnit& cross_axis_offset);

  FlexLayoutAlgorithm* algorithm;
  FlexItemVectorView line_items;
  const LayoutUnit container_logical_width;
  const LayoutUnit sum_flex_base_size;
  double total_flex_grow;
  double total_flex_shrink;
  double total_weighted_flex_shrink;
  // The hypothetical main size of an item is the flex base size clamped
  // according to its min and max main size properties
  const LayoutUnit sum_hypothetical_main_size;

  // This gets set by SetContainerMainInnerSize
  LayoutUnit container_main_inner_size;
  // initial_free_space is the initial amount of free space in this flexbox.
  // remaining_free_space starts out at the same value but as we place and lay
  // out flex items we subtract from it. Note that both values can be
  // negative.
  // These get set by FreezeInflexibleItems, see spec:
  // https://drafts.csswg.org/css-flexbox/#resolve-flexible-lengths step 3
  LayoutUnit initial_free_space;
  LayoutUnit remaining_free_space;

  // These get filled in by ComputeLineItemsPosition (for now)
  // TODO(cbiesinger): Move that to FlexibleBoxAlgorithm.
  LayoutUnit main_axis_extent;
  LayoutUnit cross_axis_offset;
  LayoutUnit cross_axis_extent;
  LayoutUnit max_ascent;
};

// This class implements the CSS Flexbox layout algorithm:
//   https://drafts.csswg.org/css-flexbox/
//
// Expected usage is as follows:
//     FlexLayoutAlgorithm algorithm(Style(), MainAxisLength(), flex_items);
//     for (each child) {
//       algorithm.emplace_back(...caller must compute these values...)
//     }
//     LayoutUnit cross_axis_offset = border + padding;
//     while ((FlexLine* line = algorithm.ComputenextLine(LogicalWidth()))) {
//       // Compute main axis size, using sum_hypothetical_main_size if
//       // indefinite
//       line->SetContainerMainInnerSize(MainAxisSize(
//           line->sum_hypothetical_main_size));
//        line->FreezeInflexibleItems();
//        while (!current_line->ResolveFlexibleLengths()) { continue; }
//        // Now, lay out the items, forcing their main axis size to
//        // item.flexed_content_size
//        LayoutUnit main_axis_offset = border + padding + scrollbar;
//        line->ComputeLineItemsPosition(main_axis_offset, cross_axis_offset);
//     }
//     // The final position of each flex item is in item.desired_location
class FlexLayoutAlgorithm {
 public:
  FlexLayoutAlgorithm(const ComputedStyle*, LayoutUnit line_break_length);

  template <typename... Args>
  FlexItem& emplace_back(Args&&... args) {
    FlexItem& item = all_items_.emplace_back(std::forward<Args>(args)...);
    item.algorithm = this;
    return item;
  }

  const ComputedStyle* Style() const { return style_; }
  const ComputedStyle& StyleRef() const { return *style_; }

  Vector<FlexLine>& FlexLines() { return flex_lines_; }

  // Computes the next flex line, stores it in FlexLines(), and returns a
  // pointer to it. Returns nullptr if there are no more lines.
  // container_logical_width is the border box width.
  FlexLine* ComputeNextFlexLine(LayoutUnit container_logical_width);

  bool IsHorizontalFlow() const;
  bool IsColumnFlow() const;
  static bool IsHorizontalFlow(const ComputedStyle&);
  bool IsLeftToRightFlow() const;
  TransformedWritingMode GetTransformedWritingMode() const;

  bool ShouldApplyMinSizeAutoForChild(const LayoutBox& child) const;

  static TransformedWritingMode GetTransformedWritingMode(const ComputedStyle&);

  static const StyleContentAlignmentData& ContentAlignmentNormalBehavior();
  static StyleContentAlignmentData ResolvedJustifyContent(const ComputedStyle&);
  static StyleContentAlignmentData ResolvedAlignContent(const ComputedStyle&);
  static ItemPosition AlignmentForChild(const ComputedStyle& flexbox_style,
                                        const ComputedStyle& child_style);

  static LayoutUnit InitialContentPositionOffset(
      LayoutUnit available_free_space,
      const StyleContentAlignmentData&,
      unsigned number_of_items);
  static LayoutUnit ContentDistributionSpaceBetweenChildren(
      LayoutUnit available_free_space,
      const StyleContentAlignmentData&,
      unsigned number_of_items);

 private:
  EOverflow MainAxisOverflowForChild(const LayoutBox& child) const;
  bool IsMultiline() const { return style_->FlexWrap() != EFlexWrap::kNowrap; }

  const ComputedStyle* style_;
  const LayoutUnit line_break_length_;
  FlexItemVector all_items_;
  Vector<FlexLine> flex_lines_;
  size_t next_item_index_;
  DISALLOW_COPY_AND_ASSIGN(FlexLayoutAlgorithm);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEXIBLE_BOX_ALGORITHM_H_
