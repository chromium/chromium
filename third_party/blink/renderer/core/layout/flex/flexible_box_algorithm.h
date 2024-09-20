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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_FLEXIBLE_BOX_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_FLEXIBLE_BOX_ALGORITHM_H_

#include "base/check_op.h"
#include "third_party/blink/renderer/core/layout/baseline_utils.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/geometry/flex_offset.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/geometry/physical_direction.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class FlexItem;
class FlexLayoutAlgorithm;
class FlexLine;
class FlexibleBoxAlgorithm;
struct MinMaxSizes;
struct NGFlexLine;

enum FlexSign {
  kPositiveFlexibility,
  kNegativeFlexibility,
};

typedef HeapVector<FlexItem, 8> FlexItemVector;

class FlexItem {
  DISALLOW_NEW();

 public:
  // Parameters:
  // - |flex_base_content_size| includes scrollbar size but not border/padding.
  // - |min_max_main_sizes| is the resolved min and max size properties in the
  //   main axis direction (not intrinsic widths). It does not include
  //   border/padding.
  //   |min_max_cross_sizes| does include cross_axis_border_padding.
  FlexItem(const FlexibleBoxAlgorithm*,
           const ComputedStyle& style,
           LayoutUnit flex_base_content_size,
           MinMaxSizes min_max_main_sizes,
           LayoutUnit main_axis_border_padding,
           PhysicalBoxStrut physical_margins,
           BoxStrut scrollbars,
           WritingMode baseline_writing_mode,
           BaselineGroup baseline_group,
           bool is_initial_block_size_indefinite,
           bool is_used_flex_basis_indefinite,
           bool depends_on_min_max_sizes);

  LayoutUnit HypotheticalMainAxisMarginBoxSize() const {
    return hypothetical_main_content_size_ + main_axis_border_padding_ +
           MainAxisMarginExtent();
  }

  LayoutUnit FlexBaseMarginBoxSize() const {
    return flex_base_content_size_ + main_axis_border_padding_ +
           MainAxisMarginExtent();
  }

  LayoutUnit FlexedBorderBoxSize() const {
    return flexed_content_size_ + main_axis_border_padding_;
  }

  LayoutUnit FlexedMarginBoxSize() const {
    return flexed_content_size_ + main_axis_border_padding_ +
           MainAxisMarginExtent();
  }

  LayoutUnit ClampSizeToMinAndMax(LayoutUnit size) const {
    return min_max_main_sizes_.ClampSizeToMinAndMax(size);
  }

  ItemPosition Alignment() const;

  bool MainAxisIsInlineAxis() const;

  // Returns the main-start margin value.
  LayoutUnit FlowAwareMarginStart() const;
  // Returns the main-end margin value.
  LayoutUnit FlowAwareMarginEnd() const;
  // Returns the cross-start margin value ignoring flex-wrap.
  LayoutUnit FlowAwareMarginBefore() const;
  // Returns the cross-end margin value ignoring flex-wrap.
  LayoutUnit FlowAwareMarginAfter() const;
  // Returns the margin value on the block-end in the container writing-mode.
  // This isn't aware of `flex-direction` and `flex-wrap`.
  LayoutUnit MarginBlockEnd() const;

  LayoutUnit MainAxisMarginExtent() const;
  LayoutUnit CrossAxisMarginExtent() const;

  LayoutUnit MarginBoxAscent(bool is_last_baseline, bool is_wrap_reverse) const;

  void UpdateAutoMarginsInMainAxis(LayoutUnit auto_margin_offset);

  // Returns true if the margins were adjusted due to auto margin resolution.
  bool UpdateAutoMarginsInCrossAxis(LayoutUnit available_alignment_space);

  LayoutUnit CrossAxisOffset(const NGFlexLine&, LayoutUnit cross_axis_size);

  static LayoutUnit AlignmentOffset(LayoutUnit available_free_space,
                                    ItemPosition position,
                                    LayoutUnit baseline_offset,
                                    bool is_wrap_reverse);

  void Trace(Visitor*) const;

  const FlexibleBoxAlgorithm* algorithm_;
  Member<const ComputedStyle> style_;
  const LayoutUnit flex_base_content_size_;
  const MinMaxSizes min_max_main_sizes_;
  const LayoutUnit hypothetical_main_content_size_;
  const LayoutUnit main_axis_border_padding_;
  PhysicalBoxStrut physical_margins_;
  const BoxStrut scrollbars_;
  const WritingDirectionMode baseline_writing_direction_;
  const BaselineGroup baseline_group_;

  LayoutUnit flexed_content_size_;

  // When set by the caller, this should be the size pre-stretching.
  LayoutUnit cross_axis_size_;

  const bool is_initial_block_size_indefinite_;
  const bool is_used_flex_basis_indefinite_;
  const bool depends_on_min_max_sizes_;
  bool frozen_;

  // The above fields are used by the flex algorithm. The following fields, by
  // contrast, are just convenient storage.
  BlockNode ng_input_node_;
  Member<const LayoutResult> layout_result_;
  std::optional<LayoutUnit> max_content_contribution_;
};

class FlexItemVectorView {
  DISALLOW_NEW();

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

  FlexItem* begin() { return vector_->data() + start_; }
  const FlexItem* begin() const { return vector_->data() + start_; }
  FlexItem* end() { return vector_->data() + end_; }
  const FlexItem* end() const { return vector_->data() + end_; }

 private:
  FlexItemVector* vector_;
  wtf_size_t start_;
  wtf_size_t end_;
};

class FlexLine {
  DISALLOW_NEW();

 public:
  typedef Vector<FlexItem*, 8> ViolationsVector;

  // This will std::move the passed-in line_items.
  FlexLine(FlexibleBoxAlgorithm* algorithm,
           FlexItemVectorView line_items,
           LayoutUnit sum_flex_base_size,
           double total_flex_grow,
           double total_flex_shrink,
           double total_weighted_flex_shrink,
           LayoutUnit sum_hypothetical_main_size)
      : algorithm_(algorithm),
        line_items_(std::move(line_items)),
        sum_flex_base_size_(sum_flex_base_size),
        total_flex_grow_(total_flex_grow),
        total_flex_shrink_(total_flex_shrink),
        total_weighted_flex_shrink_(total_weighted_flex_shrink),
        sum_hypothetical_main_size_(sum_hypothetical_main_size) {}

  FlexSign Sign() const {
    return sum_hypothetical_main_size_ < container_main_inner_size_
               ? kPositiveFlexibility
               : kNegativeFlexibility;
  }

  void SetContainerMainInnerSize(LayoutUnit size) {
    container_main_inner_size_ = size;
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
  void ComputeLineItemsPosition();

  FlexibleBoxAlgorithm* algorithm_;
  FlexItemVectorView line_items_;
  const LayoutUnit sum_flex_base_size_;
  double total_flex_grow_;
  double total_flex_shrink_;
  double total_weighted_flex_shrink_;
  // The hypothetical main size of an item is the flex base size clamped
  // according to its min and max main size properties
  const LayoutUnit sum_hypothetical_main_size_;

  // This gets set by SetContainerMainInnerSize
  LayoutUnit container_main_inner_size_;
  // initial_free_space is the initial amount of free space in this flexbox.
  // remaining_free_space starts out at the same value but as we place and lay
  // out flex items we subtract from it. Note that both values can be
  // negative.
  // These get set by FreezeInflexibleItems, see spec:
  // https://drafts.csswg.org/css-flexbox/#resolve-flexible-lengths step 3
  LayoutUnit initial_free_space_;
  LayoutUnit remaining_free_space_;

  // These get filled in by ComputeLineItemsPosition
  LayoutUnit cross_axis_extent_;

  LayoutUnit max_major_ascent_ = LayoutUnit::Min();
  LayoutUnit max_minor_ascent_ = LayoutUnit::Min();
};

// This class implements the CSS Flexbox layout algorithm:
//   https://drafts.csswg.org/css-flexbox/
//
// Expected usage is as follows:
//     FlexibleBoxAlgorithm algorithm(Style(), MainAxisLength());
//     for (each child) {
//       algorithm.emplace_back(...caller must compute these values...)
//     }
//     while ((FlexLine* line = algorithm.ComputenextLine(LogicalWidth()))) {
//       // Compute main axis size, using sum_hypothetical_main_size if
//       // indefinite
//       line->SetContainerMainInnerSize(MainAxisSize(
//           line->sum_hypothetical_main_size));
//        line->FreezeInflexibleItems();
//        while (!current_line->ResolveFlexibleLengths()) { continue; }
//        // Now, lay out the items, forcing their main axis size to
//        // item.flexed_content_size
//        line->ComputeLineItemsPosition();
//     }
// The final position of each flex item is in item.offset
class CORE_EXPORT FlexibleBoxAlgorithm {
  DISALLOW_NEW();

 public:
  FlexibleBoxAlgorithm(const ComputedStyle*,
                       LayoutUnit line_break_length,
                       LogicalSize percent_resolution_sizes,
                       Document*);
  FlexibleBoxAlgorithm(const FlexibleBoxAlgorithm&) = delete;

  ~FlexibleBoxAlgorithm() { all_items_.clear(); }
  FlexibleBoxAlgorithm& operator=(const FlexibleBoxAlgorithm&) = delete;

  template <typename... Args>
  FlexItem& emplace_back(Args&&... args) {
    return all_items_.emplace_back(this, std::forward<Args>(args)...);
  }

  wtf_size_t NumItems() const { return all_items_.size(); }

  const ComputedStyle* Style() const { return style_.Get(); }
  const ComputedStyle& StyleRef() const { return *style_; }

  const Vector<FlexLine>& FlexLines() const { return flex_lines_; }
  Vector<FlexLine>& FlexLines() { return flex_lines_; }

  // Computes the next flex line, stores it in FlexLines(), and returns a
  // pointer to it. Returns nullptr if there are no more lines.
  FlexLine* ComputeNextFlexLine();

  bool IsHorizontalFlow() const;
  bool IsColumnFlow() const;
  bool IsMultiline() const { return style_->FlexWrap() != EFlexWrap::kNowrap; }
  static bool IsHorizontalFlow(const ComputedStyle&);
  static bool IsColumnFlow(const ComputedStyle&);
  // Returns the physical direction of the main axis.
  // This function is aware of `writing-mode`, `direction`, and
  // `flex-direction`, but assumes `flex-direction:column-reverse` is same as
  // `flex-direction:column`.
  PhysicalDirection MainAxisDirection() const;
  // Returns the physical direction of the cross axis.
  // This function is aware of `writing-mode`, `flex-direction`, and
  // no `flex-wrap`.
  PhysicalDirection CrossAxisDirection() const;

  bool ShouldApplyMinSizeAutoForChild(const LayoutBox& child) const;

  // Returns the intrinsic size of this box in the block direction. Call this
  // after all flex lines have been created and processed (ie. after the
  // ComputeLineItemsPosition stage).
  // For a column flexbox, this will return the max across all flex lines of
  // the length of the line, minus any added spacing due to justification.
  // For row flexboxes, this returns the bottom (block axis) of the last flex
  // line. In both cases, border/padding is not included.
  LayoutUnit IntrinsicContentBlockSize() const;

  static const StyleContentAlignmentData& ContentAlignmentNormalBehavior();
  static StyleContentAlignmentData ResolvedJustifyContent(const ComputedStyle&);
  static StyleContentAlignmentData ResolvedAlignContent(const ComputedStyle&);
  static ItemPosition AlignmentForChild(const ComputedStyle& flexbox_style,
                                        const ComputedStyle& child_style);

  static LayoutUnit ContentDistributionSpaceBetweenChildren(
      LayoutUnit available_free_space,
      const StyleContentAlignmentData&,
      unsigned number_of_items);

  FlexItem* FlexItemAtIndex(wtf_size_t line_index, wtf_size_t item_index) const;

  static LayoutUnit GapBetweenItems(const ComputedStyle& style,
                                    LogicalSize percent_resolution_sizes);
  static LayoutUnit GapBetweenLines(const ComputedStyle& style,
                                    LogicalSize percent_resolution_sizes);

  void Trace(Visitor*) const;

  const LayoutUnit gap_between_items_;
  const LayoutUnit gap_between_lines_;

 private:
  friend class FlexLayoutAlgorithm;

  Member<const ComputedStyle> style_;
  const LayoutUnit line_break_length_;
  FlexItemVector all_items_;
  Vector<FlexLine> flex_lines_;
  wtf_size_t next_item_index_;
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::FlexItem)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_FLEXIBLE_BOX_ALGORITHM_H_
