// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_ALGORITHM_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ComputedStyle;
class NGEarlyBreak;
class NGLayoutResult;
struct MinMaxSizesInput;

// Operations provided by a layout algorithm.
class NGLayoutAlgorithmOperations {
 public:
  // Actual layout function. Lays out the children and descendants within the
  // constraints given by the NGConstraintSpace. Returns a layout result with
  // the resulting layout information.
  // TODO(layout-dev): attempt to make this function const.
  virtual scoped_refptr<const NGLayoutResult> Layout() = 0;

  // Computes the min-content and max-content intrinsic sizes for the given box.
  // The result will not take any min-width, max-width or width properties into
  // account.
  virtual MinMaxSizesResult ComputeMinMaxSizes(
      const MinMaxSizesInput&) const = 0;
};

// Parameters to pass when creating a layout algorithm for a block node.
struct NGLayoutAlgorithmParams {
  STACK_ALLOCATED();

 public:
  NGLayoutAlgorithmParams(NGBlockNode node,
                          const NGFragmentGeometry& fragment_geometry,
                          const NGConstraintSpace& space,
                          const NGBlockBreakToken* break_token = nullptr,
                          const NGEarlyBreak* early_break = nullptr)
      : node(node),
        fragment_geometry(fragment_geometry),
        space(space),
        break_token(break_token),
        early_break(early_break) {}

  NGBlockNode node;
  const NGFragmentGeometry& fragment_geometry;
  const NGConstraintSpace& space;
  const NGBlockBreakToken* break_token;
  const NGEarlyBreak* early_break;
  const NGLayoutResult* previous_result = nullptr;
};

// Base class for all LayoutNG algorithms.
template <typename NGInputNodeType,
          typename NGBoxFragmentBuilderType,
          typename NGBreakTokenType>
class CORE_EXPORT NGLayoutAlgorithm : public NGLayoutAlgorithmOperations {
  STACK_ALLOCATED();
 public:
  NGLayoutAlgorithm(NGInputNodeType node,
                    scoped_refptr<const ComputedStyle> style,
                    const NGConstraintSpace& space,
                    TextDirection direction,
                    const NGBreakTokenType* break_token)
      : node_(node),
        break_token_(break_token),
        container_builder_(node,
                           style,
                           &space,
                           {space.GetWritingMode(), direction}) {}

  // Constructor for algorithms that use NGBoxFragmentBuilder and
  // NGBlockBreakToken.
  explicit NGLayoutAlgorithm(const NGLayoutAlgorithmParams& params)
      : node_(params.node),
        break_token_(params.break_token),
        container_builder_(
            params.node,
            &params.node.Style(),
            &params.space,
            {params.space.GetWritingMode(), params.space.Direction()}) {
    container_builder_.SetIsNewFormattingContext(
        params.space.IsNewFormattingContext());
    container_builder_.SetInitialFragmentGeometry(params.fragment_geometry);
    if (UNLIKELY(params.space.HasBlockFragmentation())) {
      DCHECK(params.space.IsAnonymous() || !params.node.IsMonolithic());
      SetupFragmentBuilderForFragmentation(params.space, params.break_token,
                                           &container_builder_);
    }
  }

  virtual ~NGLayoutAlgorithm() = default;

 protected:
  const NGConstraintSpace& ConstraintSpace() const {
    DCHECK(container_builder_.ConstraintSpace());
    return *container_builder_.ConstraintSpace();
  }

  const ComputedStyle& Style() const { return node_.Style(); }

  NGBfcOffset ContainerBfcOffset() const {
    DCHECK(container_builder_.BfcBlockOffset());
    return {container_builder_.BfcLineOffset(),
            *container_builder_.BfcBlockOffset()};
  }

  NGInputNodeType Node() const { return node_; }

  const NGBreakTokenType* BreakToken() const { return break_token_.get(); }

  const NGBoxStrut& BorderPadding() const {
    return container_builder_.BorderPadding();
  }
  const NGBoxStrut& BorderScrollbarPadding() const {
    return container_builder_.BorderScrollbarPadding();
  }
  const LogicalSize& ChildAvailableSize() const {
    return container_builder_.ChildAvailableSize();
  }

  NGInputNodeType node_;

  // The break token from which we are currently resuming layout.
  scoped_refptr<const NGBreakTokenType> break_token_;

  NGBoxFragmentBuilderType container_builder_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_ALGORITHM_H_
