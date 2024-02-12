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

  // TODO(layout-dev): The rest of this function is quite mysterious. We should
  // try to get rid of it.
  const BlockBreakToken* old_fragment_break_token =
      last_fragmentainer.GetBreakToken();
  if (old_fragment_break_token) {
    container_builder_.SetHasColumnSpanner(
        old_fragment_break_token->IsCausedByColumnSpanner());
  }

  // In this algorithm we'll add all break tokens manually, to ensure that we
  // retain the original order (we may have a break before a node that precedes
  // a node which actually got a fragment). Disable the automatic child break
  // token addition that we normally get as part of adding child fragments. Note
  // that we will not add break tokens for OOFs that fragment. There's no need
  // for those break tokens, since the calling code will resume the OOFs on its
  // own.
  container_builder_.SetShouldAddBreakTokensManually();

  // Copy the original child break tokens.
  if (old_fragment_break_token) {
    for (const auto& child_break_token :
         old_fragment_break_token->ChildBreakTokens()) {
      container_builder_.AddBreakToken(child_break_token);
    }
  }
}

const LayoutResult* SimplifiedOofLayoutAlgorithm::Layout() {
  FinishFragmentationForFragmentainer(GetConstraintSpace(),
                                      &container_builder_);
  return container_builder_.ToBoxFragment();
}

void SimplifiedOofLayoutAlgorithm::AppendOutOfFlowResult(
    const LayoutResult* result) {
  container_builder_.AddResult(*result, result->OutOfFlowPositionedOffset());
}

}  // namespace blink
