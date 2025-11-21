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
class LayoutObject;

class InlineContainingBlockUtils {
  STATIC_ONLY(InlineContainingBlockUtils);

 public:
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
    LogicalOffset relative_offset;
    bool is_hidden_for_paint;
  };

  // This is only used on or by structures on the stack.
  using InlineContainingBlockMap =
      HeapHashMap<Member<const LayoutObject>,
                  std::optional<InlineContainingBlockGeometry>>;

  // Computes the geometry required for any inline containing blocks.
  // |inline_containing_block_map| is a map whose keys specify which objects we
  // need to calculate inline containing block geometry for. |container_builder|
  // is the builder of the containing block of the inline containers.
  static void ComputeInlineContainerGeometry(
      InlineContainingBlockMap* inline_containing_block_map,
      BoxFragmentBuilder* container_builder);

  // Computes the geometry required for any inline containing blocks inside a
  // fragmentation context. |box| is the containing block the inline containers
  // are descendants of. |accumulated_containing_block_size| is the size of the
  // containing block, including the total block size from all fragmentainers.
  // |inline_containing_block_map| is a map whose keys specify which objects we
  // need to calculate inline containing block geometry for.
  static void ComputeInlineContainerGeometryForFragmentainer(
      const LayoutBox* box,
      PhysicalSize accumulated_containing_block_size,
      InlineContainingBlockMap* inline_containing_block_map);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_INLINE_CONTAINING_BLOCK_UTILS_H_
