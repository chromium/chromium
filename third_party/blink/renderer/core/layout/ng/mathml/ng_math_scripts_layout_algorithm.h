// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATH_SCRIPTS_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATH_SCRIPTS_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"
#include "third_party/blink/renderer/core/mathml/mathml_scripts_element.h"

namespace blink {

class NGBlockNode;

// This algorithm handles msub, msup and msubsup elements.
class CORE_EXPORT NGMathScriptsLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  explicit NGMathScriptsLayoutAlgorithm(const NGLayoutAlgorithmParams& params);

 private:
  struct SubSupPair {
    NGBlockNode sub = nullptr;
    NGBlockNode sup = nullptr;
  };

  void GatherChildren(NGBlockNode* base,
                      Vector<SubSupPair>*,
                      NGBlockNode* prescripts,
                      unsigned* first_prescript_index,
                      NGBoxFragmentBuilder* = nullptr) const;

  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesInput&) const final;

  struct ChildAndMetrics {
    DISALLOW_NEW();

   public:
    scoped_refptr<const NGLayoutResult> result;
    LayoutUnit ascent;
    LayoutUnit descent;
    LayoutUnit inline_size;
    LayoutUnit base_italic_correction;
    NGBoxStrut margins;
    NGBlockNode node = nullptr;
  };
  typedef Vector<ChildAndMetrics, 4> ChildrenAndMetrics;

  ChildAndMetrics LayoutAndGetMetrics(NGBlockNode child) const;

  struct VerticalMetrics {
    STACK_ALLOCATED();

   public:
    LayoutUnit sub_shift;
    LayoutUnit sup_shift;
    LayoutUnit ascent;
    LayoutUnit descent;
    NGBoxStrut margins;
  };
  VerticalMetrics GetVerticalMetrics(
      const ChildAndMetrics& base_metrics,
      const ChildrenAndMetrics& sub_metrics,
      const ChildrenAndMetrics& sup_metrics) const;

  scoped_refptr<const NGLayoutResult> Layout() final;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATH_SCRIPTS_LAYOUT_ALGORITHM_H_
