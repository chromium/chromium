// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SIMPLIFIED_OOF_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SIMPLIFIED_OOF_LAYOUT_ALGORITHM_H_

#include "base/notreached.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/layout_algorithm.h"

namespace blink {

struct PhysicalFragmentLink;

// This is more a copy-and-append algorithm than a layout algorithm.
// This algorithm will only run when we are trying to add OOF-positioned
// elements to an already laid out fragmentainer. It performs a copy of the
// previous |PhysicalFragment| and appends the OOF-positioned elements to the
// |container_builder_|.
class CORE_EXPORT SimplifiedOofLayoutAlgorithm
    : public LayoutAlgorithm<BlockNode, BoxFragmentBuilder, BlockBreakToken> {
 public:
  SimplifiedOofLayoutAlgorithm(const LayoutAlgorithmParams&,
                               const PhysicalBoxFragment&,
                               bool is_new_fragment);

  const LayoutResult* Layout() override;
  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesFloatInput&) override {
    NOTREACHED();
    return MinMaxSizesResult();
  }

  void AppendOutOfFlowResult(const LayoutResult* child);

 private:
  void AddChildFragment(const PhysicalFragmentLink& old_fragment);

  const WritingDirectionMode writing_direction_;
  PhysicalSize previous_physical_container_size_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SIMPLIFIED_OOF_LAYOUT_ALGORITHM_H_
