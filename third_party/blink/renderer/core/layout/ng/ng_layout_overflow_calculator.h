// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_OVERFLOW_CALCULATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_OVERFLOW_CALCULATOR_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// This class contains the logic for correctly determining the layout-overflow
// (also known as scrollable-overflow) for a fragment.
// https://drafts.csswg.org/css-overflow-3/#scrollable
class CORE_EXPORT NGLayoutOverflowCalculator {
  STACK_ALLOCATED();

 public:
  static PhysicalRect RecalculateLayoutOverflowForFragment(
      const NGPhysicalBoxFragment&,
      bool has_block_fragmentation);

  NGLayoutOverflowCalculator(const NGBlockNode&,
                             bool is_css_box,
                             bool has_block_fragmentation,
                             const NGPhysicalBoxStrut& borders,
                             const NGPhysicalBoxStrut& scrollbar,
                             const NGPhysicalBoxStrut& padding,
                             PhysicalSize size,
                             WritingDirectionMode);

  // Applies the final adjustments given the bounds of any inflow children
  // (|inflow_bounds|), and returns the final layout-overflow.
  const PhysicalRect Result(const absl::optional<PhysicalRect> inflow_bounds);

  // Adds layout-overflow from |child_fragment|, at |offset|.
  void AddChild(const NGPhysicalBoxFragment& child_fragment,
                PhysicalOffset offset) {
    if (is_view_ && child_fragment.IsFixedPositioned())
      return;
    PhysicalRect child_overflow = LayoutOverflowForPropagation(child_fragment);
    child_overflow.offset += offset;
    AddOverflow(child_overflow, child_fragment.IsFragmentainerBox());
  }

  // Adds layout-overflow from fragment-items.
  void AddItems(const NGPhysicalBoxFragment&, const NGFragmentItems&);
  void AddItems(const LayoutObject*,
                const NGFragmentItemsBuilder::ItemWithOffsetList&);

  void AddTableSelfRect();

 private:
  template <typename Items>
  void AddItemsInternal(const LayoutObject* layout_object, const Items& items);

  PhysicalRect AdjustOverflowForHanging(const PhysicalRect& line_box_rect,
                                        PhysicalRect overflow);
  PhysicalRect AdjustOverflowForScrollOrigin(const PhysicalRect& overflow);

  PhysicalRect LayoutOverflowForPropagation(
      const NGPhysicalBoxFragment& child_fragment);

  void AddOverflow(PhysicalRect child_overflow,
                   bool child_is_fragmentainer = false) {
    if (is_scroll_container_)
      child_overflow = AdjustOverflowForScrollOrigin(child_overflow);

    // A fragmentainer may result in an overflow, even if it is empty. For
    // example, an overflow as a result of a non-zero column gap.
    if (!child_overflow.IsEmpty() || child_is_fragmentainer)
      layout_overflow_.UniteEvenIfEmpty(child_overflow);
  }

  const NGBlockNode node_;
  const WritingDirectionMode writing_direction_;
  const bool is_scroll_container_;
  const bool is_view_;
  const bool has_left_overflow_;
  const bool has_top_overflow_;
  const bool has_non_visible_overflow_;
  const bool has_block_fragmentation_;

  const NGPhysicalBoxStrut padding_;
  const PhysicalSize size_;

  PhysicalRect padding_rect_;
  PhysicalRect layout_overflow_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_OVERFLOW_CALCULATOR_H_
