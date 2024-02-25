// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/inline_containing_block_utils.h"

#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

namespace {

// std::pair.first points to the start linebox fragment.
// std::pair.second points to the end linebox fragment.
// TODO(layout-dev): Update this to a struct for increased readability.
using LineBoxPair =
    std::pair<const PhysicalLineBoxFragment*, const PhysicalLineBoxFragment*>;

// |fragment_converter| is the converter for the current containing block
// fragment, and |containing_block_converter| is the converter of the
// containing block where all fragments are stacked. These are used to
// convert offsets to be relative to the full containing block rather
// than the current containing block fragment.
template <class Items>
void GatherInlineContainerFragmentsFromItems(
    const Items& items,
    const PhysicalOffset& box_offset,
    InlineContainingBlockUtils::InlineContainingBlockMap*
        inline_containing_block_map,
    HeapHashMap<Member<const LayoutObject>, LineBoxPair>*
        containing_linebox_map,
    const WritingModeConverter* fragment_converter = nullptr,
    const WritingModeConverter* containing_block_converter = nullptr) {
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
    if (!box)
      continue;

    // See if we need the containing block information for this inline.
    const LayoutObject* key = box->GetLayoutObject();
    auto it = inline_containing_block_map->find(key);
    if (it == inline_containing_block_map->end())
      continue;

    std::optional<InlineContainingBlockUtils::InlineContainingBlockGeometry>&
        containing_block_geometry = it->value;
    LineBoxPair& containing_lineboxes =
        containing_linebox_map->insert(key, LineBoxPair{nullptr, nullptr})
            .stored_value->value;
    DCHECK(containing_block_geometry.has_value() ||
           !containing_lineboxes.first);

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

    if (containing_lineboxes.first == linebox) {
      // Unite the start rect with the fragment's rect.
      containing_block_geometry->start_fragment_union_rect.Unite(fragment_rect);
    } else if (!containing_lineboxes.first) {
      DCHECK(!containing_lineboxes.second);
      // This is the first linebox we've encountered, initialize the containing
      // block geometry.
      containing_lineboxes.first = linebox;
      containing_lineboxes.second = linebox;
      containing_block_geometry =
          InlineContainingBlockUtils::InlineContainingBlockGeometry{
              fragment_rect, fragment_rect,
              containing_block_geometry->relative_offset};
    }

    if (containing_lineboxes.second == linebox) {
      // Unite the end rect with the fragment's rect.
      containing_block_geometry->end_fragment_union_rect.Unite(fragment_rect);
    } else if (!linebox->IsEmptyLineBox()) {
      // We've found a new "end" linebox,  update the containing block geometry.
      containing_lineboxes.second = linebox;
      containing_block_geometry->end_fragment_union_rect = fragment_rect;
    }
  }
}

}  // namespace

void InlineContainingBlockUtils::ComputeInlineContainerGeometry(
    InlineContainingBlockMap* inline_containing_block_map,
    BoxFragmentBuilder* container_builder) {
  if (inline_containing_block_map->empty())
    return;

  // This function requires that we have the final size of the fragment set
  // upon the builder.
  DCHECK_GE(container_builder->InlineSize(), LayoutUnit());
  DCHECK_GE(container_builder->FragmentBlockSize(), LayoutUnit());

  HeapHashMap<Member<const LayoutObject>, LineBoxPair> containing_linebox_map;

  if (container_builder->ItemsBuilder()) {
    // To access the items correctly we need to convert them to the physical
    // coordinate space.
    DCHECK_EQ(container_builder->ItemsBuilder()->GetWritingMode(),
              container_builder->GetWritingMode());
    DCHECK_EQ(container_builder->ItemsBuilder()->Direction(),
              container_builder->Direction());
    GatherInlineContainerFragmentsFromItems(
        container_builder->ItemsBuilder()->Items(ToPhysicalSize(
            container_builder->Size(), container_builder->GetWritingMode())),
        PhysicalOffset(), inline_containing_block_map, &containing_linebox_map);
    return;
  }

  // If we have children which are anonymous block, we might contain split
  // inlines, this can occur in the following example:
  // <div>
  //    Some text <span style="position: relative;">text
  //    <div>block</div>
  //    text </span> text.
  // </div>
  for (const auto& child : container_builder->Children()) {
    if (!child.fragment->IsAnonymousBlock())
      continue;

    const auto& child_fragment = To<PhysicalBoxFragment>(*child.fragment);
    const auto* items = child_fragment.Items();
    if (!items)
      continue;

    const PhysicalOffset child_offset = child.offset.ConvertToPhysical(
        container_builder->GetWritingDirection(),
        ToPhysicalSize(container_builder->Size(),
                       container_builder->GetWritingMode()),
        child_fragment.Size());
    GatherInlineContainerFragmentsFromItems(items->Items(), child_offset,
                                            inline_containing_block_map,
                                            &containing_linebox_map);
  }
}

void InlineContainingBlockUtils::ComputeInlineContainerGeometryForFragmentainer(
    const LayoutBox* box,
    PhysicalSize accumulated_containing_block_size,
    InlineContainingBlockMap* inline_containing_block_map) {
  if (inline_containing_block_map->empty())
    return;

  WritingDirectionMode writing_direction =
      box->StyleRef().GetWritingDirection();
  WritingModeConverter containing_block_converter = WritingModeConverter(
      writing_direction, accumulated_containing_block_size);

  // Used to keep track of the block contribution from previous fragments
  // so that the child offsets are relative to the top of the containing block,
  // as if all fragments are stacked.
  LayoutUnit current_block_offset;

  HeapHashMap<Member<const LayoutObject>, LineBoxPair> containing_linebox_map;
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
    } else {
      // If we have children which are anonymous block, we might contain split
      // inlines, this can occur in the following example:
      // <div>
      //    Some text <span style="position: relative;">text
      //    <div>block</div>
      //    text </span> text.
      // </div>
      for (const auto& child : physical_fragment.Children()) {
        if (!child.fragment->IsAnonymousBlock())
          continue;

        const auto& child_fragment = To<PhysicalBoxFragment>(*child.fragment);
        if (!child_fragment.HasItems())
          continue;

        GatherInlineContainerFragmentsFromItems(
            child_fragment.Items()->Items(), child.offset + offset,
            inline_containing_block_map, &containing_linebox_map,
            &current_fragment_converter, &containing_block_converter);
      }
    }
    if (const BlockBreakToken* break_token =
            physical_fragment.GetBreakToken()) {
      current_block_offset = break_token->ConsumedBlockSize();
    }
  }
}

}  // namespace blink
