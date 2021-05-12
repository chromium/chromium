// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_UNPOSITIONED_FLOAT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_UNPOSITIONED_FLOAT_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_bfc_offset.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

class ComputedStyle;
class NGConstraintSpace;

// Struct that keeps all information needed to position floats in LayoutNG.
struct CORE_EXPORT NGUnpositionedFloat final {
  STACK_ALLOCATED();

 public:
  NGUnpositionedFloat(NGBlockNode node,
                      const NGBlockBreakToken* token,
                      const LogicalSize available_size,
                      const LogicalSize percentage_size,
                      const LogicalSize replaced_percentage_size,
                      const NGBfcOffset& origin_bfc_offset,
                      const NGConstraintSpace& parent_space,
                      const ComputedStyle& parent_style)
      : node(node),
        token(token),
        available_size(available_size),
        percentage_size(percentage_size),
        replaced_percentage_size(replaced_percentage_size),
        origin_bfc_offset(origin_bfc_offset),
        parent_space(parent_space),
        parent_style(parent_style) {}

  NGBlockNode node;
  scoped_refptr<const NGBlockBreakToken> token;

  const LogicalSize available_size;
  const LogicalSize percentage_size;
  const LogicalSize replaced_percentage_size;
  const NGBfcOffset origin_bfc_offset;
  const NGConstraintSpace& parent_space;
  const ComputedStyle& parent_style;

  // layout_result and margins are used as a cache when measuring the
  // inline_size of a float in an inline context.
  scoped_refptr<const NGLayoutResult> layout_result;
  NGBoxStrut margins;

  bool IsLineLeft(TextDirection cb_direction) const {
    return node.Style().Floating(cb_direction) == EFloat::kLeft;
  }
  bool IsLineRight(TextDirection cb_direction) const {
    return node.Style().Floating(cb_direction) == EFloat::kRight;
  }
  EClear ClearType(TextDirection cb_direction) const {
    return node.Style().Clear(cb_direction);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_UNPOSITIONED_FLOAT_H_
