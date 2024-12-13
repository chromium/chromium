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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_FLEXIBLE_BOX_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_FLEXIBLE_BOX_ALGORITHM_H_

#include "base/check_op.h"
#include "third_party/blink/renderer/core/layout/baseline_utils.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/flex/flex_item.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/geometry/physical_direction.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class FlexLayoutAlgorithm;
class FlexLine;
class FlexibleBoxAlgorithm;

typedef HeapVector<FlexItem, 8> FlexItemVector;

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

  // TODO(crbug.com/351564777): Resolve a buffer safety issue.
  FlexItem* begin() { return UNSAFE_TODO(vector_->data() + start_); }
  const FlexItem* begin() const {
    return UNSAFE_TODO(vector_->data() + start_);
  }
  FlexItem* end() { return UNSAFE_TODO(vector_->data() + end_); }
  const FlexItem* end() const { return UNSAFE_TODO(vector_->data() + end_); }

 private:
  FlexItemVector* vector_;
  wtf_size_t start_;
  wtf_size_t end_;
};

class FlexLine {
  DISALLOW_NEW();

 public:
  // This will std::move the passed-in line_items.
  FlexLine(FlexItemVectorView line_items,
           LayoutUnit sum_flex_base_size,
           LayoutUnit sum_hypothetical_main_size)
      : line_items_(std::move(line_items)),
        sum_flex_base_size_(sum_flex_base_size),
        sum_hypothetical_main_size_(sum_hypothetical_main_size) {}

  FlexItemVectorView line_items_;

  const LayoutUnit sum_flex_base_size_;
  const LayoutUnit sum_hypothetical_main_size_;
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

  wtf_size_t NumItems() const { return all_items_.size(); }

  // Computes the next flex line, and returns a pointer to it.
  // Returns nullptr if there are no more lines.
  FlexLine* ComputeNextFlexLine(bool is_mutli_line);

  static bool IsHorizontalFlow(const ComputedStyle&);
  static bool IsColumnFlow(const ComputedStyle&);

  static const StyleContentAlignmentData& ContentAlignmentNormalBehavior();
  static StyleContentAlignmentData ResolvedJustifyContent(const ComputedStyle&);
  static StyleContentAlignmentData ResolvedAlignContent(const ComputedStyle&);
  static ItemPosition AlignmentForChild(const ComputedStyle& flexbox_style,
                                        const ComputedStyle& child_style);

  static LayoutUnit AlignmentOffset(LayoutUnit available_free_space,
                                    ItemPosition position,
                                    LayoutUnit baseline_offset,
                                    bool is_wrap_reverse);

  static LayoutUnit ContentDistributionSpaceBetweenChildren(
      LayoutUnit available_free_space,
      const StyleContentAlignmentData&,
      unsigned number_of_items);

  const FlexItem& FlexItemAtIndex(wtf_size_t item_index) const;

  static LayoutUnit GapBetweenItems(const ComputedStyle& style,
                                    LogicalSize percent_resolution_sizes);
  static LayoutUnit GapBetweenLines(const ComputedStyle& style,
                                    LogicalSize percent_resolution_sizes);

  void Trace(Visitor*) const;

  const LayoutUnit gap_between_items_;
  const LayoutUnit gap_between_lines_;

 private:
  friend class FlexLayoutAlgorithm;

  const LayoutUnit line_break_length_;
  FlexItemVector all_items_;
  Vector<FlexLine> flex_lines_;
  wtf_size_t next_item_index_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_FLEXIBLE_BOX_ALGORITHM_H_
