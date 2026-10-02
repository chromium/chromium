// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_INLINE_CONTAINING_BLOCK_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_INLINE_CONTAINING_BLOCK_UTILS_H_

#include <optional>

#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class BoxFragmentBuilder;
class LayoutBox;
class LayoutInline;

// Inline containing block geometry is defined by two rectangles, generated
// by fragments of the LayoutInline.
struct InlineContainingBlockGeometry {
  DISALLOW_NEW();
  // Union of fragments generated on the first line.
  PhysicalRect start_fragment_union_rect;
  // Union of fragments generated on the last line.
  PhysicalRect end_fragment_union_rect;
  // The accumulated relative offset of the inline container to be applied to
  // any descendants after fragmentation.
  //
  // TODO(crbug.com/40267498): Remove along with non-FragmentedOofInCb code.
  LogicalOffset relative_offset;
  bool is_hidden_due_to_layout;
};

// Containing block information for each LayoutInline that contain out-of-flow
// positioned descendants.
//
// This is only used on or by structures on the stack.
using InlineContainingBlockMap =
    HeapHashMap<Member<const LayoutInline>,
                std::optional<InlineContainingBlockGeometry>>;

// Compute the containing block geometry for all inlines that have out-of-flow
// positioned descendants that depend on it.
void ComputeInlineContainerGeometry(const BoxFragmentBuilder&,
                                    InlineContainingBlockMap*);

// Computes the geometry required for any inline containing blocks inside a
// fragmentation context. |box| is the containing block the inline containers
// are descendants of. |accumulated_containing_block_size| is the size of the
// containing block, including the total block size from all fragmentainers.
// |inline_containing_block_map| is a map whose keys specify which objects we
// need to calculate inline containing block geometry for.
void ComputeInlineContainerGeometryForFragmentainer(
    const LayoutBox* box,
    PhysicalSize accumulated_containing_block_size,
    InlineContainingBlockMap* inline_containing_block_map);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_INLINE_CONTAINING_BLOCK_UTILS_H_
