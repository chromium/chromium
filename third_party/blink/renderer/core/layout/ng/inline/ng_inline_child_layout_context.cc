// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_child_layout_context.h"

namespace blink {

namespace {

struct SameSizeAsNGInlineChildLayoutContext {
  base::Optional<NGInlineLayoutStateStack> box_states_;
  void* pointers[2];
  unsigned number;
};

static_assert(
    sizeof(NGInlineChildLayoutContext) ==
        sizeof(SameSizeAsNGInlineChildLayoutContext),
    "Only data which can be regenerated from the node, constraints, and break "
    "token are allowed to be placed in this context object.");

}  // namespace

NGInlineLayoutStateStack*
NGInlineChildLayoutContext::BoxStatesIfValidForItemIndex(
    const Vector<NGInlineItem>& items,
    unsigned item_index) {
  if (box_states_.has_value() && items_ == &items && item_index_ == item_index)
    return &*box_states_;
  return nullptr;
}

}  // namespace blink
