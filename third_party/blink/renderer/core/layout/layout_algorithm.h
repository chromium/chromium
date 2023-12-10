// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ColumnSpannerPath;
class ComputedStyle;
class EarlyBreak;
class LayoutResult;

// Operations provided by a layout algorithm.
class LayoutAlgorithmOperations {
 public:
  // Actual layout function. Lays out the children and descendants within the
  // constraints given by the ConstraintSpace. Returns a layout result with
  // the resulting layout information.
  // TODO(layout-dev): attempt to make this function const.
  virtual const LayoutResult* Layout() = 0;

  // Computes the min-content and max-content intrinsic sizes for the given box.
  // The result will not take any min-width, max-width or width properties into
  // account.
  virtual MinMaxSizesResult ComputeMinMaxSizes(
      const MinMaxSizesFloatInput&) = 0;
};

// Parameters to pass when creating a layout algorithm for a block node.
struct LayoutAlgorithmParams {
  STACK_ALLOCATED();

 public:
  LayoutAlgorithmParams(
      BlockNode node,
      const FragmentGeometry& fragment_geometry,
      const ConstraintSpace& space,
      const BlockBreakToken* break_token = nullptr,
      const EarlyBreak* early_break = nullptr,
      const HeapVector<Member<EarlyBreak>>* additional_early_breaks = nullptr)
      : node(node),
        fragment_geometry(fragment_geometry),
        space(space),
        break_token(break_token),
        early_break(early_break),
        additional_early_breaks(additional_early_breaks) {}

  BlockNode node;
  const FragmentGeometry& fragment_geometry;
  const ConstraintSpace& space;
  const BlockBreakToken* break_token;
  const EarlyBreak* early_break;
  const ColumnSpannerPath* column_spanner_path = nullptr;
  const LayoutResult* previous_result = nullptr;
  const HeapVector<Member<EarlyBreak>>* additional_early_breaks;
};

// Base class for all LayoutNG algorithms.
template <typename InputNodeType,
          typename BoxFragmentBuilderType,
          typename BreakTokenType>
class CORE_EXPORT LayoutAlgorithm : public LayoutAlgorithmOperations {
  STACK_ALLOCATED();
 public:
  LayoutAlgorithm(InputNodeType node,
                  const ComputedStyle* style,
                  const ConstraintSpace& space,
                  TextDirection direction,
                  const BreakTokenType* break_token)
      : node_(node),
        break_token_(break_token),
        container_builder_(node,
                           style,
                           space,
                           {space.GetWritingMode(), direction}) {}

  // Constructor for algorithms that use BoxFragmentBuilder and
  // BlockBreakToken.
  explicit LayoutAlgorithm(const LayoutAlgorithmParams& params)
      : node_(To<InputNodeType>(params.node)),
        early_break_(params.early_break),
        break_token_(params.break_token),
        container_builder_(
            params.node,
            &params.node.Style(),
            params.space,
            {params.space.GetWritingMode(), params.space.Direction()}),
        additional_early_breaks_(params.additional_early_breaks) {
    container_builder_.SetIsNewFormattingContext(
        params.space.IsNewFormattingContext());
    container_builder_.SetInitialFragmentGeometry(params.fragment_geometry);
    if (UNLIKELY(params.space.HasBlockFragmentation() ||
                 IsBreakInside(params.break_token))) {
      SetupFragmentBuilderForFragmentation(
          params.space, params.node, params.break_token, &container_builder_);
    }
  }

  virtual ~LayoutAlgorithm() = default;

 protected:
  const ConstraintSpace& GetConstraintSpace() const {
    return container_builder_.GetConstraintSpace();
  }

  const ComputedStyle& Style() const { return node_.Style(); }

  BfcOffset ContainerBfcOffset() const {
    DCHECK(container_builder_.BfcBlockOffset());
    return {container_builder_.BfcLineOffset(),
            *container_builder_.BfcBlockOffset()};
  }

  const InputNodeType& Node() const { return node_; }

  const BreakTokenType* GetBreakToken() const { return break_token_; }

  const BoxStrut& Borders() const { return container_builder_.Borders(); }
  const BoxStrut& Padding() const { return container_builder_.Padding(); }
  const BoxStrut& BorderPadding() const {
    return container_builder_.BorderPadding();
  }
  const BoxStrut& BorderScrollbarPadding() const {
    return container_builder_.BorderScrollbarPadding();
  }
  LayoutUnit OriginalBorderScrollbarPaddingBlockStart() const {
    return container_builder_.OriginalBorderScrollbarPaddingBlockStart();
  }
  const LogicalSize& ChildAvailableSize() const {
    return container_builder_.ChildAvailableSize();
  }

  ExclusionSpace& GetExclusionSpace() {
    return container_builder_.GetExclusionSpace();
  }

  // Lay out again, this time with a predefined good breakpoint that we
  // discovered in the first pass. This happens when we run out of space in a
  // fragmentainer at an less-than-ideal location, due to breaking restrictions,
  // such as orphans, widows, break-before:avoid or break-after:avoid.
  template <typename Algorithm>
  const LayoutResult* RelayoutAndBreakEarlier(
      const EarlyBreak& breakpoint,
      const HeapVector<Member<EarlyBreak>>* additional_early_breaks = nullptr) {
    // Not allowed to recurse!
    DCHECK(!early_break_);
    DCHECK(!additional_early_breaks_ || additional_early_breaks_->empty());

    LayoutAlgorithmParams params(Node(),
                                 container_builder_.InitialFragmentGeometry(),
                                 GetConstraintSpace(), GetBreakToken(),
                                 &breakpoint, additional_early_breaks);
    Algorithm algorithm_with_break(params);
    return RelayoutAndBreakEarlier(&algorithm_with_break);
  }

  template <typename Algorithm>
  const LayoutResult* RelayoutAndBreakEarlier(Algorithm* new_algorithm) {
    DCHECK(new_algorithm);
    auto& new_builder = new_algorithm->container_builder_;
    new_builder.SetBoxType(container_builder_.BoxType());
    // We're not going to run out of space in the next layout pass, since we're
    // breaking earlier, so no space shortage will be detected. Repeat what we
    // found in this pass.
    new_builder.PropagateSpaceShortage(
        container_builder_.MinimalSpaceShortage());
    return new_algorithm->Layout();
  }

  // Lay out again, this time without block fragmentation. This happens when a
  // block-axis clipped node reaches the end, but still has content inside that
  // wants to break. We don't want any zero-sized clipped fragments that
  // contribute to superfluous fragmentainers.
  template <typename Algorithm>
  const LayoutResult* RelayoutWithoutFragmentation() {
    DCHECK(GetConstraintSpace().HasBlockFragmentation());
    // We'll relayout with a special cloned constraint space that disables
    // further fragmentation (but rather lets clipped child content "overflow"
    // past the fragmentation line). This means that the cached constraint space
    // will still be set up to do block fragmentation, but that should be the
    // right thing, since, as far as input is concerned, this node is meant to
    // perform block fragmentation (and it may already have produced multiple
    // fragment, but this one will be the last).
    ConstraintSpace new_space =
        GetConstraintSpace().CloneWithoutFragmentation();

    LayoutAlgorithmParams params(Node(),
                                 container_builder_.InitialFragmentGeometry(),
                                 new_space, GetBreakToken());
    Algorithm algorithm_without_fragmentation(params);
    auto& new_builder = algorithm_without_fragmentation.container_builder_;
    new_builder.SetBoxType(container_builder_.BoxType());
    return algorithm_without_fragmentation.Layout();
  }

  InputNodeType node_;

  // When set, this will specify where to break before or inside. If not set,
  // the algorithm will need to figure out where to break on its own.
  const EarlyBreak* early_break_ = nullptr;

  // The break token from which we are currently resuming layout.
  const BreakTokenType* break_token_;

  BoxFragmentBuilderType container_builder_;

  // There are cases where we may need more than one early break per fragment.
  // For example, there may be an early break within multiple flex columns. This
  // can be used to pass additional early breaks to the next layout pass.
  const HeapVector<Member<EarlyBreak>>* additional_early_breaks_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_ALGORITHM_H_
