// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FRAME_SET_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FRAME_SET_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"

namespace blink {

class HTMLDimension;
class NGBlockBreakToken;

class CORE_EXPORT NGFrameSetLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  explicit NGFrameSetLayoutAlgorithm(const NGLayoutAlgorithmParams& params);

  const NGLayoutResult* Layout() override;
  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesFloatInput&) override;

 private:
  Vector<LayoutUnit> LayoutAxis(wtf_size_t count,
                                const Vector<HTMLDimension>& grid,
                                const Vector<int>& deltas,
                                LayoutUnit available_length);
  void LayoutChildren(const FrameSetLayoutData& layout_data,
                      const PhysicalSize& frameset_size);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FIELDSET_LAYOUT_ALGORITHM_H_
