// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/simplified_oof_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"

namespace blink {

SimplifiedOofLayoutAlgorithm::SimplifiedOofLayoutAlgorithm(
    const LayoutAlgorithmParams& params,
    const PhysicalBoxFragment& last_fragmentainer)
    : LayoutAlgorithm(params) {
  DCHECK(last_fragmentainer.IsFragmentainerBox());
  DCHECK(params.space.HasKnownFragmentainerBlockSize());

  container_builder_.SetBoxType(last_fragmentainer.GetBoxType());
  container_builder_.SetPageNameIfNeeded(last_fragmentainer.PageName());
  container_builder_.SetFragmentBlockSize(
      params.space.FragmentainerBlockSize());
  container_builder_.SetHasOutOfFlowFragmentChild(true);
}

void SimplifiedOofLayoutAlgorithm::ResumeColumnLayout(
    const BlockBreakToken* old_fragment_break_token) {
  if (!old_fragment_break_token ||
      !old_fragment_break_token->IsCausedByColumnSpanner()) {
    return;
  }

  // Since the last column break was caused by a spanner, and we're about to add
  // additional columns now, we have some work to do: In order to correctly
  // resume layout after the spanner after having added additional columns to
  // hold OOFs, we need to copy over any in-flow child break tokens, so that the
  // outgoing break token from the last column before the spanner actually
  // points at the content that we're supposed to resume at after the spanner.
  for (const auto& child_break_token :
       old_fragment_break_token->ChildBreakTokens()) {
    if (!child_break_token->InputNode().IsOutOfFlowPositioned()) {
      container_builder_.AddBreakToken(child_break_token);
    }
  }

  // Carry over the IsCausedByColumnSpanner flag (stored in the break token).
  container_builder_.SetHasColumnSpanner(true);
}

const LayoutResult* SimplifiedOofLayoutAlgorithm::Layout() {
  FinishFragmentationForFragmentainer(&container_builder_);
  return container_builder_.ToBoxFragment();
}

void SimplifiedOofLayoutAlgorithm::AppendOutOfFlowResult(
    const LayoutResult* result) {
  container_builder_.AddResult(*result, result->OutOfFlowPositionedOffset());
}

}  // namespace blink
