// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/space_utils.h"

#include "third_party/blink/renderer/core/html/forms/html_button_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/geometry/bfc_offset.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

bool AdjustToClearance(LayoutUnit clearance_offset, BfcOffset* offset) {
  DCHECK(offset);
  if (clearance_offset > offset->block_offset) {
    offset->block_offset = clearance_offset;
    return true;
  }

  return false;
}

void SetOrthogonalFallbackInlineSize(const ComputedStyle& parent_style,
                                     const LayoutInputNode child,
                                     ConstraintSpaceBuilder* builder) {
  DCHECK(!IsParallelWritingMode(parent_style.GetWritingMode(),
                                child.Style().GetWritingMode()));

  PhysicalSize orthogonal_children_containing_block_size =
      child.InitialContainingBlockSize();

  LayoutUnit fallback_size =
      parent_style.IsHorizontalWritingMode()
          ? orthogonal_children_containing_block_size.height
          : orthogonal_children_containing_block_size.width;

  LayoutUnit size(LayoutUnit::Max());
  if (parent_style.LogicalHeight().IsFixed()) {
    // Note that during layout, fixed size will already be taken care of (and
    // set in the constraint space), but when calculating intrinsic sizes of
    // orthogonal children, that won't be the case.
    size = LayoutUnit(parent_style.LogicalHeight().Pixels());
  }
  if (parent_style.LogicalMaxHeight().IsFixed()) {
    size = std::min(size, LayoutUnit(parent_style.LogicalMaxHeight().Pixels()));
  }
  if (parent_style.LogicalMinHeight().IsFixed()) {
    size = std::max(size, LayoutUnit(parent_style.LogicalMinHeight().Pixels()));
  }
  // Calculate the content-box size.
  if (parent_style.BoxSizing() == EBoxSizing::kBorderBox) {
    // We're unable to resolve percentages at this point, so make sure we're
    // only dealing with fixed-size values.
    if (!parent_style.PaddingBlockStart().IsFixed() ||
        !parent_style.PaddingBlockEnd().IsFixed()) {
      builder->SetOrthogonalFallbackInlineSize(fallback_size);
      return;
    }

    LayoutUnit border_padding(parent_style.BorderBlockStartWidth() +
                              parent_style.BorderBlockEndWidth() +
                              parent_style.PaddingBlockStart().Pixels() +
                              parent_style.PaddingBlockEnd().Pixels());

    size -= border_padding;
    size = size.ClampNegativeToZero();
  }

  fallback_size = std::min(fallback_size, size);
  builder->SetOrthogonalFallbackInlineSize(fallback_size);
}

bool ShouldBlockContainerChildStretchAutoInlineSize(const BlockNode& child) {
  if (child.IsReplaced()) {
    return false;
  }
  if (child.IsTable()) {
    return false;
  }
  if (const auto* node = child.GetDOMNode()) {
    if (IsA<HTMLButtonElement>(node) || IsA<HTMLInputElement>(node) ||
        IsA<HTMLSelectElement>(node) || IsA<HTMLTextAreaElement>(node)) {
      return false;
    }
  }
  return true;
}

void SetTextBoxTrimOnChildSpaceBuilder(
    const BoxFragmentBuilder& fragment_builder,
    bool known_to_have_successive_content,
    ConstraintSpaceBuilder* space_builder) {
  space_builder->SetShouldTextBoxTrimNodeStart(
      fragment_builder.ShouldTextBoxTrimNodeStart());
  space_builder->SetShouldTextBoxTrimFragmentainerStart(
      fragment_builder.ShouldTextBoxTrimFragmentainerStart());
  space_builder->SetShouldTextBoxTrimFragmentainerEnd(
      fragment_builder.ShouldTextBoxTrimFragmentainerEnd());
  if (fragment_builder.ShouldTextBoxTrimEnd()) {
    // Attempt to trim the end unless we know for sure that there's content to
    // follow.
    space_builder->SetShouldTextBoxTrimNodeEnd(
        fragment_builder.ShouldTextBoxTrimNodeEnd() &&
        !known_to_have_successive_content);
  }
}

}  // namespace blink
