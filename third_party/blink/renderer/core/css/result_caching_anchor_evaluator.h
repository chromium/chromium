// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESULT_CACHING_ANCHOR_EVALUATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESULT_CACHING_ANCHOR_EVALUATOR_H_

#include <optional>
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/anchor_evaluator.h"

namespace blink {

class AnchorResults;

// An implementation of AnchorEvaluator which returns the results of
// the specified evaluator, but also stores the results in the specified
// AnchorResults object.
//
// This class is instantiated during interleaved style recalc from
// out-of-flow layout (StyleEngine::UpdateStyleForOutOfFlow),
// and only used by style resolutions during that function.
//
// See also AnchorResults.
class ResultCachingAnchorEvaluator : public AnchorEvaluator {
  STACK_ALLOCATED();

 public:
  ResultCachingAnchorEvaluator(AnchorEvaluator*, AnchorResults&);

  std::optional<LayoutUnit> Evaluate(
      const AnchorQuery&,
      const ScopedCSSName* position_anchor,
      const std::optional<InsetAreaOffsets>&) override;
  std::optional<InsetAreaOffsets> ComputeInsetAreaOffsetsForLayout(
      const ScopedCSSName* position_anchor,
      InsetArea inset_area) override;
  std::optional<PhysicalOffset> ComputeAnchorCenterOffsets(
      const ComputedStyleBuilder&) override;

 private:
  AnchorEvaluator* evaluator_;
  AnchorResults& results_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESULT_CACHING_ANCHOR_EVALUATOR_H_
