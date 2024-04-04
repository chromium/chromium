// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_PAGINATED_ROOT_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_PAGINATED_ROOT_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/layout_algorithm.h"

namespace blink {

class BlockBreakToken;
class BlockNode;
class ConstraintSpace;
struct LogicalSize;

class CORE_EXPORT PaginatedRootLayoutAlgorithm
    : public LayoutAlgorithm<BlockNode, BoxFragmentBuilder, BlockBreakToken> {
 public:
  explicit PaginatedRootLayoutAlgorithm(const LayoutAlgorithmParams& params);

  const LayoutResult* Layout();

  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesFloatInput&) {
    NOTREACHED_NORETURN();
  }

  // Create an empty page box fragment, modeled after an existing fragmentainer.
  // The resulting page box may then be used and mutated by the out-of-flow
  // layout code, to add out-of-flow descendants.
  static const PhysicalBoxFragment& CreateEmptyPage(
      const BlockNode& node,
      const ConstraintSpace& parent_space,
      const PhysicalBoxFragment& previous_fragmentainer);

 private:
  const PhysicalBoxFragment* LayoutPage(
      uint32_t page_index,
      const AtomicString& page_name,
      const BlockBreakToken* break_token) const;
  static ConstraintSpace CreateConstraintSpaceForPages(const BlockNode&,
                                                       const ConstraintSpace&,
                                                       const LogicalSize& size);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_PAGINATED_ROOT_LAYOUT_ALGORITHM_H_
