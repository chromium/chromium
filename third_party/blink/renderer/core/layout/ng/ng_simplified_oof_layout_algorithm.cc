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
  if (is_new_fragment) {
    children_ = {};
    iterator_ = children_.end();
    return;
  }

  // We need the previous physical container size to calculate the position of
  // any child fragments.
  previous_physical_container_size_ = fragment.Size();

  // The OOF fragments need to be added in a particular order that matches the
  // order of break tokens. Here's a list of rules to follow , in order:
  // 1. If there are any already appended fragments that are a continuation of
  //    layout (i.e. they are the Nth fragment of a layout box, where N > 1),
  //    they should be appended before anything else.
  // 2. If we're trying to append a fragment that is a continuation of layout
  //    for an OOF node (from AppendOutOfFlowResult), add it after the fragments
  //    added in step 1.
  // 3. Add the remaining children that were not appended during step 1.
  // 4. Add the OOF fragments that were not a continuation of layout, the ones
  //    that weren't appended in step 2.
  children_ = fragment.Children();
  iterator_ = children_.begin();

  while (iterator_ != children_.end()) {
    const auto& child_link = *iterator_;
    const auto* child_fragment = To<NGPhysicalBoxFragment>(child_link.get());
    if (!child_fragment->IsFirstForNode()) {
      AddChildFragment(child_link);
      iterator_++;
    } else {
      // We can break here because fragments that are a continuation of layout
      // are always the first children of a physical fragment.
      break;
    }
  }
}

scoped_refptr<const NGLayoutResult> NGSimplifiedOOFLayoutAlgorithm::Layout() {
  // There might not be any children to append, whether it's because we are in a
  // new fragmentainer or because they have all been added in Step 1.
  if (iterator_ != children_.end()) {
    // Step 3: Add the remaining children that were not added in step 1.
    while (iterator_ != children_.end()) {
      const auto& child_link = *iterator_;
      const auto* child_fragment = To<NGPhysicalBoxFragment>(child_link.get());
      DCHECK(child_fragment->IsFirstForNode());
      AddChildFragment(child_link);
      iterator_++;
    }

    // Step 4: Add the OOF fragments that aren't a continuation of layout.
    for (auto result : remaining_oof_results_) {
      container_builder_.AddResult(*result,
                                   result->OutOfFlowPositionedOffset());
    }
  }
  return container_builder_.ToBoxFragment();
}

void NGSimplifiedOOFLayoutAlgorithm::AppendOutOfFlowResult(
    scoped_refptr<const NGLayoutResult> result) {
  // Add the new result directly to the builder when the fragment of the result
  // to append is not the first fragment of its corresponding layout box,
  // meaning that it's positioned directly at the start of the fragmentainer.
  // This ensures that we keep the fragments and the break tokens in order.
  //
  // Also add the result directly to the builder if there are no more children
  // to append. This can happen when all children have been added in Step 1 or
  // when we are in a new fragmentainer since a new fragmentainer doesn't have
  // any child.
  if (iterator_ == children_.end() ||
      !To<NGPhysicalBoxFragment>(result->PhysicalFragment()).IsFirstForNode()) {
    // Step 2: Add the fragments that are a continuation of layout directly to
    // the builder.
    container_builder_.AddResult(*result, result->OutOfFlowPositionedOffset());
    return;
  }
  // Since there is no previous break token associated with the first fragment
  // of a fragmented OOF element, we cannot append this result before any other
  // children of this fragmentainer. Keep the order by adding it after.
  remaining_oof_results_.push_back(result);
}

void NGSimplifiedOOFLayoutAlgorithm::AddChildFragment(const NGLink& child) {
  const auto* fragment = To<NGPhysicalContainerFragment>(child.get());
  // Determine the previous position in the logical coordinate system.
  LogicalOffset child_offset =
      WritingModeConverter(writing_direction_,
                           previous_physical_container_size_)
          .ToLogical(child.Offset(), fragment->Size());

  // Add the fragment to the builder.
  container_builder_.AddChild(*fragment, child_offset);
}

}  // namespace blink
