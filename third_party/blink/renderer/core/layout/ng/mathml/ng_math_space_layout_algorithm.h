// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATH_SPACE_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATH_SPACE_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"

namespace blink {

class CORE_EXPORT NGMathSpaceLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  explicit NGMathSpaceLayoutAlgorithm(const NGLayoutAlgorithmParams& params);

 private:
  scoped_refptr<const NGLayoutResult> Layout() final;

  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesInput&) const final;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATH_SPACE_LAYOUT_ALGORITHM_H_
