// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_child_layout_context.h"

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"

namespace blink {

namespace {

struct SameSizeAsNGInlineChildLayoutContext {
  NGLogicalLineItems line_items_;
  base::Optional<NGInlineLayoutStateStack> box_states_;
  void* pointers[2];
  unsigned number;
  Vector<scoped_refptr<const NGBlockBreakToken>> propagated_float_break_tokens_;
};

static_assert(
    sizeof(NGInlineChildLayoutContext) ==
        sizeof(SameSizeAsNGInlineChildLayoutContext),
    "Only data which can be regenerated from the node, constraints, and break "
    "token are allowed to be placed in this context object.");

}  // namespace

NGInlineChildLayoutContext::NGInlineChildLayoutContext() = default;
NGInlineChildLayoutContext::~NGInlineChildLayoutContext() = default;

NGInlineLayoutStateStack*
NGInlineChildLayoutContext::BoxStatesIfValidForItemIndex(
    const Vector<NGInlineItem>& items,
    unsigned item_index) {
  if (box_states_.has_value() && items_ == &items && item_index_ == item_index)
    return &*box_states_;
  return nullptr;
}

void NGInlineChildLayoutContext::ClearPropagatedBreakTokens() {
  propagated_float_break_tokens_.Shrink(0);
}

void NGInlineChildLayoutContext::PropagateBreakToken(
    scoped_refptr<const NGBlockBreakToken> token) {
  propagated_float_break_tokens_.push_back(token);
}

}  // namespace blink
