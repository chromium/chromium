// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/inline_containing_block_utils.h"

#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

struct LineBoxRange {
  DISALLOW_NEW();

 public:
  LineBoxRange() = default;
  LineBoxRange(const PhysicalLineBoxFragment* start,
               const PhysicalLineBoxFragment* end)
      : start(start), end(end) {}

  void Trace(Visitor* visitor) const {
    visitor->Trace(start);
    visitor->Trace(end);
  }

  Member<const PhysicalLineBoxFragment> start;
  Member<const PhysicalLineBoxFragment> end;
};

using ContainingLineBoxMap =
    HeapHashMap<Member<const LayoutInline>, LineBoxRange>;

// |fragment_converter| is the converter for the current containing block
// fragment, and |containing_block_converter| is the converter of the
// containing block where all fragments are stacked. These are used to
// convert offsets to be relative to the full containing block rather
// than the current containing block fragment.
template <class Items>
void GatherInlineContainerFragmentsFromItems(
    const Items& items,
    PhysicalOffset box_offset,
    InlineContainingBlockMap* inline_containing_block_map,
    ContainingLineBoxMap* containing_linebox_map,
    const WritingModeConverter* fragment_converter = nullptr,
    const WritingModeConverter* containing_block_converter = nullptr) {
  DCHECK(!fragment_converter ||
         !RuntimeEnabledFeatures::FragmentedOofInCbEnabled());
  DCHECK_EQ(!!fragment_converter, !!containing_block_converter);
  const PhysicalLineBoxFragment* linebox = nullptr;
  for (const auto& item : items) {
    // Track the current linebox.
    if (const PhysicalLineBoxFragment* current_linebox =
            item->LineBoxFragment()) {
      linebox = current_linebox;
      continue;
    }

    // We only care about inlines which have generated a box fragment.
    const PhysicalBoxFragment* box = item->BoxFragment();
    if (!box ||
        (box->IsOpaque() &&
         RuntimeEnabledFeatures::InlineContainingBlockSkipOpaqueEnabled())) {
      continue;
    }

    const auto* key = DynamicTo<LayoutInline>(box->GetLayoutObject());
    if (!key) {
      continue;
    }
    // See if we need the containing block information for this inline.
    auto it = inline_containing_block_map->find(key);
    if (it == inline_containing_block_map->end())
      continue;

    std::optional<InlineContainingBlockGeometry>& containing_block_geometry =
        it->value;
    LineBoxRange& containing_lineboxes =
        containing_linebox_map->insert(key, LineBoxRange{nullptr, nullptr})
            .stored_value->value;
    DCHECK(containing_block_geometry.has_value() ||
           !containing_lineboxes.start);

    PhysicalRect fragment_rect = item->RectInContainerFragment();
    if (fragment_converter) {
      // Convert the offset to be relative to the containing block such
      // that all containing block fragments are stacked.
      fragment_rect.offset = containing_block_converter->ToPhysical(
          fragment_converter->ToLogical(fragment_rect.offset,
                                        fragment_rect.size),
          fragment_rect.size);
    }
    fragment_rect.offset += box_offset;

    if (containing_lineboxes.start == linebox) {
      // Unite the start rect with the fragment's rect.
      containing_block_geometry->start_fragment_union_rect.Unite(fragment_rect);
    } else if (!containing_lineboxes.start) {
      DCHECK(!containing_lineboxes.end);
      // This is the first linebox we've encountered, initialize the containing
      // block geometry.
      containing_lineboxes.start = containing_lineboxes.end = linebox;

      // An abspos is hidden in line-clamp iff its containing block is hidden.
      // For inline CBs, this means iff all of its fragments are hidden.
      // If the first line box of an inline CB is hidden after the clamp point,
      // all of its following line boxes will be hidden, and so will all of the
      // CB's fragments.
      bool should_hide_abspos = linebox->IsHiddenDueToLayout();

      containing_block_geometry = InlineContainingBlockGeometry{
          fragment_rect, fragment_rect,
          containing_block_geometry->relative_offset,
          /*is_hidden_due_to_layout=*/should_hide_abspos};
    }

    if (containing_lineboxes.end == linebox) {
      // Unite the end rect with the fragment's rect.
      containing_block_geometry->end_fragment_union_rect.Unite(fragment_rect);
    } else if (!linebox->IsEmptyLineBox()) {
      // We've found a new "end" linebox,  update the containing block geometry.
      containing_lineboxes.end = linebox;
      containing_block_geometry->end_fragment_union_rect = fragment_rect;
    }
  }
}

}  // namespace

void ComputeInlineContainerGeometry(
    const BoxFragmentBuilder& container_builder,
    InlineContainingBlockMap* inline_containing_block_map) {
  DCHECK(!inline_containing_block_map->empty());
  DCHECK(container_builder.ItemsBuilder());

  // This function requires that we have the final size of the fragment set
  // upon the builder.
  DCHECK_GE(container_builder.InlineSize(), LayoutUnit());
  DCHECK_GE(container_builder.FragmentBlockSize(), LayoutUnit());

  // TODO(crbug.com/40267498): Move this into
  // GatherInlineContainerFragmentsFromItems() when non-FragmentedOofInCb
  // support is removed.
  ContainingLineBoxMap containing_linebox_map;

  // To access the items correctly we need to convert them to the physical
  // coordinate space.
  DCHECK_EQ(container_builder.ItemsBuilder()->GetWritingMode(),
            container_builder.GetWritingMode());
  DCHECK_EQ(container_builder.ItemsBuilder()->Direction(),
            container_builder.Direction());

  GatherInlineContainerFragmentsFromItems(
      container_builder.ItemsBuilder()->Items(), PhysicalOffset(),
      inline_containing_block_map, &containing_linebox_map);
}

void ComputeInlineContainerGeometryForFragmentainer(
    const LayoutBox* box,
    PhysicalSize accumulated_containing_block_size,
    InlineContainingBlockMap* inline_containing_block_map) {
  DCHECK(!RuntimeEnabledFeatures::FragmentedOofInCbEnabled());
  DCHECK(!inline_containing_block_map->empty());

  WritingDirectionMode writing_direction =
      box->StyleRef().GetWritingDirection();
  WritingModeConverter containing_block_converter = WritingModeConverter(
      writing_direction, accumulated_containing_block_size);

  // Used to keep track of the block contribution from previous fragments
  // so that the child offsets are relative to the top of the containing block,
  // as if all fragments are stacked.
  LayoutUnit current_block_offset;

  ContainingLineBoxMap containing_linebox_map;
  for (auto& physical_fragment : box->PhysicalFragments()) {
    LogicalOffset logical_offset(LayoutUnit(), current_block_offset);
    PhysicalOffset offset = containing_block_converter.ToPhysical(
        logical_offset, accumulated_containing_block_size);

    WritingModeConverter current_fragment_converter =
        WritingModeConverter(writing_direction, physical_fragment.Size());
    if (physical_fragment.HasItems()) {
      GatherInlineContainerFragmentsFromItems(
          physical_fragment.Items()->Items(), offset,
          inline_containing_block_map, &containing_linebox_map,
          &current_fragment_converter, &containing_block_converter);
    }
    if (const BlockBreakToken* break_token =
            physical_fragment.GetBreakToken()) {
      current_block_offset = break_token->ConsumedBlockSize();
    }
  }
}

}  // namespace blink
