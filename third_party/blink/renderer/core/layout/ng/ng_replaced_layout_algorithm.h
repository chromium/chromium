// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_REPLACED_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_REPLACED_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"

namespace blink {

class NGBlockNode;
class NGBlockBreakToken;

class CORE_EXPORT NGReplacedLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  explicit NGReplacedLayoutAlgorithm(const NGLayoutAlgorithmParams& params);

  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesInput&) const override;
  scoped_refptr<const NGLayoutResult> Layout() override;

 private:
  const LogicalSize natural_size_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_REPLACED_LAYOUT_ALGORITHM_H_
