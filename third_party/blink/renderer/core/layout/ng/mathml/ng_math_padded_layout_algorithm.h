// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATH_PADDED_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATH_PADDED_LAYOUT_ALGORITHM_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_row_layout_algorithm.h"

namespace blink {

class CORE_EXPORT NGMathPaddedLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  explicit NGMathPaddedLayoutAlgorithm(const NGLayoutAlgorithmParams& params);

  const NGLayoutResult* Layout() final;

  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesFloatInput&) final;

 private:
  LayoutUnit RequestedLSpace() const;
  LayoutUnit RequestedVOffset() const;
  absl::optional<LayoutUnit> RequestedAscent(LayoutUnit content_ascent) const;
  absl::optional<LayoutUnit> RequestedDescent(LayoutUnit content_descent) const;

  void GetContentAsAnonymousMrow(NGBlockNode* content) const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATH_PADDED_LAYOUT_ALGORITHM_H_
