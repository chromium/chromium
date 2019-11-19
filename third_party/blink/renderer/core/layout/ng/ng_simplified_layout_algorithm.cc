// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_simplified_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"

namespace blink {

NGSimplifiedLayoutAlgorithm::NGSimplifiedLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params,
    const NGLayoutResult& result)
    : NGLayoutAlgorithm(params),
      previous_result_(result),
      border_scrollbar_padding_(params.fragment_geometry.border +
                                params.fragment_geometry.scrollbar +
                                params.fragment_geometry.padding),
      writing_mode_(Style().GetWritingMode()),
      direction_(Style().Direction()),
      exclusion_space_(ConstraintSpace().ExclusionSpace()) {
  // Currently this only supports block-flow layout due to the static-position
  // calculations. If support for other layout types is added this logic will
  // need to be changed.
  DCHECK(Node().IsBlockFlow());
  const NGPhysicalBoxFragment& physical_fragment =
      To<NGPhysicalBoxFragment>(result.PhysicalFragment());

  container_builder_.SetStyleVariant(physical_fragment.StyleVariant());
  container_builder_.SetIsNewFormattingContext(
      physical_fragment.IsBlockFormattingContextRoot());
  container_builder_.SetInitialFragmentGeometry(params.fragment_geometry);

  NGExclusionSpace exclusion_space = result.ExclusionSpace();
  container_builder_.SetExclusionSpace(std::move(exclusion_space));

  // Ensure that the parent layout hasn't asked us to move our BFC position.
  DCHECK_EQ(ConstraintSpace().BfcOffset(),
            previous_result_.GetConstraintSpaceForCaching().BfcOffset());

  if (result.SubtreeModifiedMarginStrut())
    container_builder_.SetSubtreeModifiedMarginStrut();

  container_builder_.SetBfcLineOffset(result.BfcLineOffset());
  if (result.BfcBlockOffset())
    container_builder_.SetBfcBlockOffset(*result.BfcBlockOffset());

  container_builder_.SetEndMarginStrut(result.EndMarginStrut());
  container_builder_.SetIntrinsicBlockSize(result.IntrinsicBlockSize());
  container_builder_.SetUnpositionedListMarker(result.UnpositionedListMarker());

  if (result.IsSelfCollapsing())
    container_builder_.SetIsSelfCollapsing();
  if (result.IsPushedByFloats())
    container_builder_.SetIsPushedByFloats();
  container_builder_.SetAdjoiningObjectTypes(result.AdjoiningObjectTypes());

  for (const auto& request : ConstraintSpace().BaselineRequests()) {
    base::Optional<LayoutUnit> baseline = physical_fragment.Baseline(request);
    if (baseline)
      container_builder_.AddBaseline(request, *baseline);
  }

  container_builder_.SetBlockSize(ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(),
      container_builder_.Borders() + container_builder_.Padding(),
      result.IntrinsicBlockSize()));

  child_available_inline_size_ =
      ShrinkAvailableSize(container_builder_.InitialBorderBoxSize(),
                          border_scrollbar_padding_)
          .inline_size;

  // We need the previous physical container size to calculate the position of
  // any child fragments.
  previous_physical_container_size_ = physical_fragment.Size();

  // The static-position needs to account for any intrinsic-padding.
  if (ConstraintSpace().IsTableCell()) {
    border_scrollbar_padding_ += ComputeIntrinsicPadding(
        ConstraintSpace(), Style(), container_builder_.Scrollbar());
  }
}

scoped_refptr<const NGLayoutResult> NGSimplifiedLayoutAlgorithm::Layout() {
  // Since simplified layout's |Layout()| function deals with laying out
  // children, we can early out if we are display-locked.
  if (Node().LayoutBlockedByDisplayLock(DisplayLockLifecycleTarget::kChildren))
    return container_builder_.ToBoxFragment();

  const auto previous_child_fragments =
      To<NGPhysicalBoxFragment>(previous_result_.PhysicalFragment()).Children();

  auto it = previous_child_fragments.begin();
  auto end = previous_child_fragments.end();

  // We may have a list-marker as our first child. This may have been
  // propagated up to this container by an arbitrary child. As we don't know
  // where it came from initially add it as the first child again.
  if (it != end && (*it)->IsListMarker()) {
    AddChildFragment(*it, *To<NGPhysicalContainerFragment>(it->get()));
    ++it;
  }

  // Initialize the static block-offset for any OOF-positioned children.
  static_block_offset_ = border_scrollbar_padding_.block_start;

  for (NGLayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    // We've already dealt with any list-markers, so just skip this node.
    if (child.IsListMarker())
      continue;

    if (child.IsOutOfFlowPositioned()) {
      HandleOutOfFlowPositioned(To<NGBlockNode>(child));
      continue;
    }

    DCHECK(it != end);

    if (child.IsInline()) {
      // Simplified layout will only run if none of the lineboxes are dirty.
      while (it != end && (*it)->IsLineBox()) {
        // NOTE: When we remove continuations it'll be necessary for lineboxes
        // to keep track of any exclusions they've added (and update the
        // exclusion space).
        AddChildFragment(*it, *To<NGPhysicalContainerFragment>(it->get()));
        ++it;
      }
      continue;
    }

    DCHECK_EQ((*it)->GetLayoutObject(), child.GetLayoutBox());

    // Add the (potentially updated) layout result.
    scoped_refptr<const NGLayoutResult> result =
        To<NGBlockNode>(child).SimplifiedLayout();

    // The child may have failed "simplified" layout! (Due to adding/removing
    // scrollbars). In this case we also return a nullptr, indicating a full
    // layout is required.
    if (!result)
      return nullptr;

    const NGPhysicalContainerFragment& fragment = result->PhysicalFragment();
    AddChildFragment(*it, fragment);

    // Update the static block-offset for any OOF-positioned children.
    // Only consider inflow children (floats don't contribute to the intrinsic
    // block-size).
    if (!child.IsFloating()) {
      const ComputedStyle& child_style = child.Style();
      NGBoxStrut child_margins = ComputeMarginsFor(
          child_style, child_available_inline_size_, writing_mode_, direction_);

      NGMarginStrut margin_strut = result->EndMarginStrut();
      margin_strut.Append(child_margins.block_end,
                          child_style.HasMarginBeforeQuirk());
      static_block_offset_ += margin_strut.Sum();
    }

    // Only take exclusion spaces from children which don't establish their own
    // formatting context.
    if (!fragment.IsBlockFormattingContextRoot())
      exclusion_space_ = result->ExclusionSpace();
    ++it;
  }

  NGOutOfFlowLayoutPart(
      Node(), ConstraintSpace(),
      container_builder_.Borders() + container_builder_.Scrollbar(),
      &container_builder_)
      .Run();

  return container_builder_.ToBoxFragment();
}

void NGSimplifiedLayoutAlgorithm::HandleOutOfFlowPositioned(
    const NGBlockNode& child) {
  LogicalOffset static_offset = {border_scrollbar_padding_.inline_start,
                                 static_block_offset_};

  if (child.Style().IsOriginalDisplayInlineType()) {
    NGBfcOffset origin_bfc_offset = {
        container_builder_.BfcLineOffset() +
            border_scrollbar_padding_.LineLeft(direction_),
        container_builder_.BfcBlockOffset().value_or(
            ConstraintSpace().ExpectedBfcBlockOffset()) +
            static_block_offset_};

    static_offset.inline_offset += CalculateOutOfFlowStaticInlineLevelOffset(
        Style(), origin_bfc_offset, exclusion_space_,
        child_available_inline_size_);
  }

  container_builder_.AddOutOfFlowChildCandidate(child, static_offset);
}

void NGSimplifiedLayoutAlgorithm::AddChildFragment(
    const NGLink& old_fragment,
    const NGPhysicalContainerFragment& new_fragment) {
  DCHECK_EQ(old_fragment->Size(), new_fragment.Size());

  PhysicalSize physical_child_size = new_fragment.Size();
  LogicalSize child_size = physical_child_size.ConvertToLogical(writing_mode_);

  // Determine the previous position in the logical coordinate system.
  LogicalOffset child_offset = old_fragment.Offset().ConvertToLogical(
      writing_mode_, direction_, previous_physical_container_size_,
      physical_child_size);

  // Add the new fragment to the builder.
  container_builder_.AddChild(new_fragment, child_offset);

  if (!new_fragment.IsFloating()) {
    // Update the static block-offset for any OOF-positioned children.
    // Only consider inflow children (floats don't contribute to the intrinsic
    // block-size).
    static_block_offset_ = child_offset.block_offset + child_size.block_size;
  } else {
    // We need to add the float to the exclusion space so that any inline-level
    // OOF-positioned nodes can correctly determine their static-position.
    const ComputedStyle& child_style = new_fragment.Style();
    NGBoxStrut child_margins = ComputeMarginsFor(
        child_style, child_available_inline_size_, writing_mode_, direction_);

    LayoutUnit child_line_offset = IsLtr(direction_)
                                       ? child_offset.inline_offset
                                       : container_builder_.InlineSize() -
                                             child_size.inline_size -
                                             child_offset.inline_offset;

    NGBfcOffset container_bfc_offset = {
        container_builder_.BfcLineOffset(),
        container_builder_.BfcBlockOffset().value_or(
            ConstraintSpace().ExpectedBfcBlockOffset())};

    // Determine the offsets for the exclusion (the margin-box of the float).
    NGBfcOffset start_offset = {
        container_bfc_offset.line_offset + child_line_offset -
            child_margins.LineLeft(direction_),
        container_bfc_offset.block_offset + child_offset.block_offset -
            child_margins.block_start};
    NGBfcOffset end_offset = {
        start_offset.line_offset +
            (child_size.inline_size + child_margins.InlineSum())
                .ClampNegativeToZero(),
        start_offset.block_offset +
            (child_size.block_size + child_margins.BlockSum())
                .ClampNegativeToZero()};

    exclusion_space_.Add(
        NGExclusion::Create(NGBfcRect(start_offset, end_offset),
                            child_style.Floating(Style()), nullptr));
  }
}

}  // namespace blink
