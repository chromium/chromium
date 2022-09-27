// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATH_SCRIPTS_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATH_SCRIPTS_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/core_export.h"
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

  struct ChildAndMetrics {
    DISALLOW_NEW();

   public:
    Member<const NGLayoutResult> result;
    LayoutUnit ascent;
    LayoutUnit descent;
    LayoutUnit inline_size;
    LayoutUnit base_italic_correction;
    NGBoxStrut margins;
    NGBlockNode node = nullptr;

    void Trace(Visitor* visitor) const {
      visitor->Trace(result);
      visitor->Trace(node);
    }
  };

  struct SubSupPair {
    DISALLOW_NEW();

   public:
    void Trace(Visitor* visitor) const {
      visitor->Trace(sub);
      visitor->Trace(sup);
    }

    NGBlockNode sub = nullptr;
    NGBlockNode sup = nullptr;
  };

 private:
  void GatherChildren(NGBlockNode* base,
                      HeapVector<SubSupPair>*,
                      NGBlockNode* prescripts,
                      unsigned* first_prescript_index,
                      NGBoxFragmentBuilder* = nullptr) const;

  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesFloatInput&) final;

  typedef HeapVector<ChildAndMetrics, 4> ChildrenAndMetrics;

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

  const NGLayoutResult* Layout() final;
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::NGMathScriptsLayoutAlgorithm::ChildAndMetrics)
WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::NGMathScriptsLayoutAlgorithm::SubSupPair)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATH_SCRIPTS_LAYOUT_ALGORITHM_H_
