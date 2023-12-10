// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_UNPOSITIONED_FLOAT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_UNPOSITIONED_FLOAT_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/geometry/bfc_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/box_strut.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

class ComputedStyle;
class ConstraintSpace;

// Struct that keeps all information needed to position floats in LayoutNG.
struct CORE_EXPORT UnpositionedFloat final {
  STACK_ALLOCATED();

 public:
  UnpositionedFloat(BlockNode node,
                    const BlockBreakToken* token,
                    const LogicalSize available_size,
                    const LogicalSize percentage_size,
                    const LogicalSize replaced_percentage_size,
                    const BfcOffset& origin_bfc_offset,
                    const ConstraintSpace& parent_space,
                    const ComputedStyle& parent_style)
      : node(node),
        token(token),
        available_size(available_size),
        percentage_size(percentage_size),
        replaced_percentage_size(replaced_percentage_size),
        origin_bfc_offset(origin_bfc_offset),
        parent_space(parent_space),
        parent_style(parent_style) {}

  BlockNode node;
  const BlockBreakToken* token = nullptr;

  const LogicalSize available_size;
  const LogicalSize percentage_size;
  const LogicalSize replaced_percentage_size;
  const BfcOffset origin_bfc_offset;
  const ConstraintSpace& parent_space;
  const ComputedStyle& parent_style;

  // layout_result and margins are used as a cache when measuring the
  // inline_size of a float in an inline context.
  const LayoutResult* layout_result = nullptr;
  BoxStrut margins;

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

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_UNPOSITIONED_FLOAT_H_
