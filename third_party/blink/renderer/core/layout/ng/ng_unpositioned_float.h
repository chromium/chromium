// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_UNPOSITIONED_FLOAT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_UNPOSITIONED_FLOAT_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

// Struct that keeps all information needed to position floats in LayoutNG.
struct CORE_EXPORT NGUnpositionedFloat final {
  DISALLOW_NEW();

 public:
  NGUnpositionedFloat(NGBlockNode node, const NGBlockBreakToken* token)
      : node(node), token(token) {}

  NGUnpositionedFloat(NGUnpositionedFloat&&) noexcept = default;
  NGUnpositionedFloat(const NGUnpositionedFloat&) noexcept = default;
  NGUnpositionedFloat& operator=(NGUnpositionedFloat&&) = default;
  NGUnpositionedFloat& operator=(const NGUnpositionedFloat&) = default;

  bool operator==(const NGUnpositionedFloat& other) const {
    return node == other.node && token == other.token;
  }

  NGBlockNode node;
  scoped_refptr<const NGBlockBreakToken> token;

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
