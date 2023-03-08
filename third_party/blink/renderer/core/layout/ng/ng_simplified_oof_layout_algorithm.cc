// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_simplified_oof_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"

namespace blink {

NGSimplifiedOOFLayoutAlgorithm::NGSimplifiedOOFLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params,
    const NGPhysicalBoxFragment& previous_fragment,
    bool is_new_fragment)
    : NGLayoutAlgorithm(params),
      writing_direction_(Style().GetWritingDirection()) {
  DCHECK(previous_fragment.IsFragmentainerBox());
  DCHECK(params.space.HasKnownFragmentainerBlockSize());

  container_builder_.SetBoxType(previous_fragment.BoxType());
  container_builder_.SetPageNameIfNeeded(previous_fragment.PageName());
  container_builder_.SetFragmentBlockSize(
      params.space.FragmentainerBlockSize());
  container_builder_.SetDisableOOFDescendantsPropagation();
  container_builder_.SetHasOutOfFlowFragmentChild(true);

  const NGBlockBreakToken* old_fragment_break_token =
      previous_fragment.BreakToken();
  if (old_fragment_break_token) {
    container_builder_.SetHasColumnSpanner(
        old_fragment_break_token->IsCausedByColumnSpanner());
  }

  // We need the previous physical container size to calculate the position of
  // any child fragments.
  previous_physical_container_size_ = previous_fragment.Size();

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
         old_fragment_break_token->ChildBreakTokens())
      container_builder_.AddBreakToken(child_break_token);
  }

  // Don't apply children to new fragments.
  if (is_new_fragment) {
    container_builder_.SetIsFirstForNode(false);
    return;
  }

  container_builder_.SetIsFirstForNode(previous_fragment.IsFirstForNode());

  // Copy the original child fragments. See above: this will *not* add the
  // outgoing break tokens from the fragments (if any).
  for (const auto& child_link : previous_fragment.Children())
    AddChildFragment(child_link);

  // Inflow-bounds should never exist on a fragmentainer.
  DCHECK(!previous_fragment.InflowBounds());
  container_builder_.SetMayHaveDescendantAboveBlockStart(
      previous_fragment.MayHaveDescendantAboveBlockStart());
}

const NGLayoutResult* NGSimplifiedOOFLayoutAlgorithm::Layout() {
  FinishFragmentationForFragmentainer(ConstraintSpace(), &container_builder_);
  return container_builder_.ToBoxFragment();
}

void NGSimplifiedOOFLayoutAlgorithm::AppendOutOfFlowResult(
    const NGLayoutResult* result) {
  container_builder_.AddResult(*result, result->OutOfFlowPositionedOffset());
}

void NGSimplifiedOOFLayoutAlgorithm::AddChildFragment(const NGLink& child) {
  const auto* fragment = child.get();
  // Determine the previous position in the logical coordinate system.
  LogicalOffset child_offset =
      WritingModeConverter(writing_direction_,
                           previous_physical_container_size_)
          .ToLogical(child.Offset(), fragment->Size());
  // Any relative offset will have already been applied, avoid re-adding one.
  absl::optional<LogicalOffset> relative_offset = LogicalOffset();

  // Add the fragment to the builder.
  container_builder_.AddChild(
      *fragment, child_offset, /* margin_strut */ nullptr,
      /* is_self_collapsing */ false, relative_offset,
      /* inline_container */ nullptr,
      /* adjustment_for_oof_propagation */ absl::nullopt);
}

}  // namespace blink
