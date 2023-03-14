// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_child_layout_context.h"

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"

namespace blink {

namespace {

struct SameSizeAsNGInlineChildLayoutContext {
  STACK_ALLOCATED();

 public:
  NGFragmentItemsBuilder items_builder_;
  absl::optional<NGInlineLayoutStateStack> box_states_;
  absl::optional<LayoutUnit> optional_layout_unit;
  void* pointers[3];
  unsigned number;
  HeapVector<Member<const NGBlockBreakToken>> propagated_float_break_tokens_;
};

static_assert(
    sizeof(NGInlineChildLayoutContext) ==
        sizeof(SameSizeAsNGInlineChildLayoutContext),
    "Only data which can be regenerated from the node, constraints, and break "
    "token are allowed to be placed in this context object.");

}  // namespace

NGInlineChildLayoutContext::NGInlineChildLayoutContext(
    const NGInlineNode& node,
    NGBoxFragmentBuilder* container_builder)
    : container_builder_(container_builder),
      items_builder_(node, container_builder->GetWritingDirection()) {
  container_builder->SetItemsBuilder(ItemsBuilder());
}

NGInlineChildLayoutContext::~NGInlineChildLayoutContext() {
  container_builder_->SetItemsBuilder(nullptr);
  propagated_float_break_tokens_.clear();
}

NGInlineLayoutStateStack*
NGInlineChildLayoutContext::BoxStatesIfValidForItemIndex(
    const HeapVector<NGInlineItem>& items,
    unsigned item_index) {
  if (box_states_.has_value() && items_ == &items && item_index_ == item_index)
    return &*box_states_;
  return nullptr;
}

void NGInlineChildLayoutContext::ClearPropagatedBreakTokens() {
  propagated_float_break_tokens_.Shrink(0);
}

void NGInlineChildLayoutContext::PropagateBreakToken(
    const NGBlockBreakToken* token) {
  propagated_float_break_tokens_.push_back(token);
}

}  // namespace blink
