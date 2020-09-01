// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_simplified_oof_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

NGSimplifiedOOFLayoutAlgorithm::NGSimplifiedOOFLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params,
    const NGPhysicalBoxFragment& fragment,
    bool is_new_fragment)
    : NGLayoutAlgorithm(params),
      writing_direction_(Style().GetWritingDirection()) {
  DCHECK(fragment.IsFragmentainerBox());
  DCHECK(params.space.HasKnownFragmentainerBlockSize());

  container_builder_.SetBoxType(fragment.BoxType());
  container_builder_.SetFragmentBlockSize(
      params.space.FragmentainerBlockSize());

  // Don't apply children to new fragments.
  if (is_new_fragment)
    return;

  // We need the previous physical container size to calculate the position of
  // any child fragments.
  previous_physical_container_size_ = fragment.Size();

  // The OOF fragments need to be added after the already existing child
  // fragments. Add them now so they are added before we append the OOF results.
  for (const auto& child_link : fragment.Children()) {
    AddChildFragment(child_link,
                     *To<NGPhysicalContainerFragment>(child_link.get()));
  }
}

scoped_refptr<const NGLayoutResult> NGSimplifiedOOFLayoutAlgorithm::Layout() {
  return container_builder_.ToBoxFragment();
}

void NGSimplifiedOOFLayoutAlgorithm::AppendOutOfFlowResult(
    scoped_refptr<const NGLayoutResult> result,
    LogicalOffset offset) {
  // Add the new result to the builder.
  container_builder_.AddResult(*result, offset);
}

void NGSimplifiedOOFLayoutAlgorithm::AddChildFragment(
    const NGLink& old_fragment,
    const NGPhysicalContainerFragment& new_fragment) {
  DCHECK_EQ(old_fragment->Size(), new_fragment.Size());

  // Determine the previous position in the logical coordinate system.
  LogicalOffset child_offset =
      WritingModeConverter(writing_direction_,
                           previous_physical_container_size_)
          .ToLogical(old_fragment.Offset(), new_fragment.Size());

  // Add the new fragment to the builder.
  container_builder_.AddChild(new_fragment, child_offset);
}

}  // namespace blink
