// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFragmentationUtils_h
#define NGFragmentationUtils_h

#include "third_party/blink/renderer/core/layout/ng/ng_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class NGConstraintSpace;

// Return the total amount of block space spent on a node by fragments
// preceding this one (but not including this one).
LayoutUnit PreviouslyUsedBlockSpace(const NGConstraintSpace&,
                                    const NGPhysicalFragment&);

// Return true if the specified fragment is the first generated fragment of
// some node.
inline bool IsFirstFragment(const NGConstraintSpace& constraint_space,
                            const NGPhysicalFragment& fragment) {
  // TODO(mstensho): Figure out how to behave for non-box fragments here. How
  // can we tell whether it's the first one? Looking for previously used block
  // space certainly isn't the answer.
  if (!fragment.IsBox())
    return true;
  return PreviouslyUsedBlockSpace(constraint_space, fragment) <= LayoutUnit();
}

// Return true if the specified fragment is the final fragment of some node.
inline bool IsLastFragment(const NGPhysicalFragment& fragment) {
  const auto* break_token = fragment.BreakToken();
  return !break_token || break_token->IsFinished();
}

// Join two adjacent break values specified on break-before and/or break-
// after. avoid* values win over auto values, and forced break values win over
// avoid* values. |first_value| is specified on an element earlier in the flow
// than |second_value|. This method is used at class A break points [1], to join
// the values of the previous break-after and the next break-before, to figure
// out whether we may, must, or should not break at that point. It is also used
// when propagating break-before values from first children and break-after
// values on last children to their container.
//
// [1] https://drafts.csswg.org/css-break/#possible-breaks
EBreakBetween JoinFragmentainerBreakValues(EBreakBetween first_value,
                                           EBreakBetween second_value);

// Return true if the specified break value has a forced break effect in the
// current fragmentation context.
bool IsForcedBreakValue(const NGConstraintSpace&, EBreakBetween);

// Return true if we are to ignore the block-start margin of the child. At the
// start of fragmentainers, in-flow block-start margins are ignored, unless
// we're right after a forced break.
// https://drafts.csswg.org/css-break/#break-margins
bool ShouldIgnoreBlockStartMargin(const NGConstraintSpace&,
                                  NGLayoutInputNode,
                                  const NGBreakToken*);

}  // namespace blink

#endif  // NGFragmentationUtils_h
