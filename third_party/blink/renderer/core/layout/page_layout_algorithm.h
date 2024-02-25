// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_PAGE_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_PAGE_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/layout_algorithm.h"

namespace blink {

class BlockBreakToken;
class BlockNode;
class ConstraintSpace;
struct LogicalSize;

class CORE_EXPORT PageLayoutAlgorithm
    : public LayoutAlgorithm<BlockNode, BoxFragmentBuilder, BlockBreakToken> {
 public:
  explicit PageLayoutAlgorithm(const LayoutAlgorithmParams& params);

  const LayoutResult* Layout() override;

  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesFloatInput&) override;

 private:
  const PhysicalBoxFragment* LayoutPage(
      uint32_t page_index,
      const AtomicString& page_name,
      const BlockBreakToken* break_token) const;
  ConstraintSpace CreateConstraintSpaceForPages(const LogicalSize& size) const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_PAGE_LAYOUT_ALGORITHM_H_
