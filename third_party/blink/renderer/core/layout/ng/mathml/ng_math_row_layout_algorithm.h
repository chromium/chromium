// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATH_ROW_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATH_ROW_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"

namespace blink {

class LayoutUnit;

class CORE_EXPORT NGMathRowLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  explicit NGMathRowLayoutAlgorithm(const NGLayoutAlgorithmParams& params);

  struct ChildWithOffsetAndMargins {
    DISALLOW_NEW();
    ChildWithOffsetAndMargins(const NGBlockNode& child,
                              const NGBoxStrut& margins,
                              LogicalOffset offset,
                              scoped_refptr<const NGLayoutResult> result)
        : child(child),
          margins(margins),
          offset(offset),
          result(std::move(result)) {}

    NGBlockNode child;
    NGBoxStrut margins;
    LogicalOffset offset;
    scoped_refptr<const NGLayoutResult> result;
  };
  typedef Vector<ChildWithOffsetAndMargins, 4> ChildrenVector;

 private:
  scoped_refptr<const NGLayoutResult> Layout() final;

  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesFloatInput&) final;

  void LayoutRowItems(ChildrenVector*,
                      LayoutUnit* max_row_block_baseline,
                      LogicalSize* row_total_size);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATH_ROW_LAYOUT_ALGORITHM_H_
