// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_INLINE_CONTAINING_BLOCK_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_INLINE_CONTAINING_BLOCK_UTILS_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class LayoutBox;
class LayoutObject;
class LayoutUnit;
class NGBoxFragmentBuilder;

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
  };

  using InlineContainingBlockMap =
      HashMap<const LayoutObject*,
              absl::optional<InlineContainingBlockGeometry>>;

  // Computes the geometry required for any inline containing blocks.
  // |inline_containing_block_map| is a map whose keys specify which objects we
  // need to calculate inline containing block geometry for. |container_builder|
  // is the builder of the containing block of the inline containers.
  static void ComputeInlineContainerGeometry(
      InlineContainingBlockMap* inline_containing_block_map,
      NGBoxFragmentBuilder* container_builder);

  // Computes the geometry required for any inline containing blocks inside a
  // fragmentation context. |box| is the containing block the inline containers
  // are descendants of. |block_offset| is the offset of the containing block to
  // the top of the first fragmentainer that it was found in.
  // |accumulated_containing_block_size| is the size of the containing block,
  // including the total block size from all fragmentainers. |container_builder|
  // is the builder of the fragmentation context root. |fragment_index| is the
  // index of the child in |container_builder| that the containing block was
  // first found in. |inline_containing_block_map| is a map whose keys specify
  // which objects we need to calculate inline containing block geometry for.
  static void ComputeInlineContainerGeometryForFragmentainer(
      const LayoutBox* box,
      LayoutUnit block_offset,
      PhysicalSize accumulated_containing_block_size,
      const NGBoxFragmentBuilder& container_builder,
      wtf_size_t fragment_index,
      InlineContainingBlockMap* inline_containing_block_map);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_INLINE_CONTAINING_BLOCK_UTILS_H_
