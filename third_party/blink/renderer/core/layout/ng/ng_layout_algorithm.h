// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutAlgorithm_h
#define NGLayoutAlgorithm_h

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/min_max_size.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class ComputedStyle;
class NGLayoutResult;
struct MinMaxSizeInput;

// Base class for all LayoutNG algorithms.
template <typename NGInputNodeType,
          typename NGBoxFragmentBuilderType,
          typename NGBreakTokenType>
class CORE_EXPORT NGLayoutAlgorithm {
  STACK_ALLOCATED();
 public:
  NGLayoutAlgorithm(NGInputNodeType node,
                    scoped_refptr<const ComputedStyle> style,
                    const NGConstraintSpace& space,
                    TextDirection direction,
                    const NGBreakTokenType* break_token)
      : node_(node),
        constraint_space_(space),
        break_token_(break_token),
        container_builder_(node, style, space.GetWritingMode(), direction) {}

  NGLayoutAlgorithm(NGInputNodeType node,
                    const NGConstraintSpace& space,
                    const NGBreakTokenType* break_token)
      : NGLayoutAlgorithm(node,
                          &node.Style(),
                          space,
                          space.Direction(),
                          break_token) {}

  virtual ~NGLayoutAlgorithm() = default;

  // Actual layout function. Lays out the children and descendants within the
  // constraints given by the NGConstraintSpace. Returns a layout result with
  // the resulting layout information.
  // TODO(layout-dev): attempt to make this function const.
  virtual scoped_refptr<NGLayoutResult> Layout() = 0;

  // Computes the min-content and max-content intrinsic sizes for the given box.
  // The result will not take any min-width, max-width or width properties into
  // account. If the return value is empty, the caller is expected to synthesize
  // this value from the overflow rect returned from Layout called with an
  // available width of 0 and LayoutUnit::max(), respectively.
  virtual base::Optional<MinMaxSize> ComputeMinMaxSize(
      const MinMaxSizeInput&) const {
    return base::nullopt;
  }

 protected:
  const NGConstraintSpace& ConstraintSpace() const { return constraint_space_; }

  const ComputedStyle& Style() const { return node_.Style(); }

  NGBfcOffset ContainerBfcOffset() const {
    DCHECK(container_builder_.BfcBlockOffset());
    return {container_builder_.BfcLineOffset(),
            container_builder_.BfcBlockOffset().value()};
  }

  NGInputNodeType Node() const { return node_; }

  const NGBreakTokenType* BreakToken() const { return break_token_; }

  NGInputNodeType node_;
  const NGConstraintSpace& constraint_space_;

  // The break token from which we are currently resuming layout.
  const NGBreakTokenType* break_token_;

  NGBoxFragmentBuilderType container_builder_;
};

}  // namespace blink

#endif  // NGLayoutAlgorithm_h
