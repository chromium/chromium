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
  void* pointers[5];
  unsigned number;
  HeapVector<Member<const NGBlockBreakToken>> tokens_;
};

static_assert(
    sizeof(NGInlineChildLayoutContext) ==
        sizeof(SameSizeAsNGInlineChildLayoutContext),
    "Only data which can be regenerated from the node, constraints, and break "
    "token are allowed to be placed in this context object.");

// Return true if we're inside a fragmentainer with known block-size (i.e. not
// if we're in an initial column balancing pass, in which case the fragmentainer
// block-size would be unconstrained). This information will be used to
// determine whether it's reasonable to pre-allocate a buffer for all the
// estimated fragment items inside the node.
bool IsBlockFragmented(const NGBoxFragmentBuilder& fragment_builder) {
  const NGConstraintSpace& space = fragment_builder.ConstraintSpace();
  return space.HasBlockFragmentation() &&
         space.HasKnownFragmentainerBlockSize();
}

}  // namespace

NGInlineChildLayoutContext::NGInlineChildLayoutContext(
    const NGInlineNode& node,
    NGBoxFragmentBuilder* container_builder,
    NGLineInfo* line_info)
    : container_builder_(container_builder),
      items_builder_(node,
                     container_builder->GetWritingDirection(),
                     IsBlockFragmented(*container_builder)),
      line_info_(line_info) {
  container_builder->SetItemsBuilder(ItemsBuilder());
}

NGInlineChildLayoutContext::NGInlineChildLayoutContext(
    const NGInlineNode& node,
    NGBoxFragmentBuilder* container_builder,
    NGScoreLineBreakContext* score_line_break_context)
    : container_builder_(container_builder),
      items_builder_(node,
                     container_builder->GetWritingDirection(),
                     IsBlockFragmented(*container_builder)),
      score_line_break_context_(score_line_break_context) {
  container_builder->SetItemsBuilder(ItemsBuilder());
}

NGInlineChildLayoutContext::~NGInlineChildLayoutContext() {
  container_builder_->SetItemsBuilder(nullptr);
  parallel_flow_break_tokens_.clear();
}

NGInlineLayoutStateStack*
NGInlineChildLayoutContext::BoxStatesIfValidForItemIndex(
    const HeapVector<NGInlineItem>& items,
    unsigned item_index) {
  if (box_states_.has_value() && items_ == &items && item_index_ == item_index)
    return &*box_states_;
  return nullptr;
}

void NGInlineChildLayoutContext::ClearParallelFlowBreakTokens() {
  parallel_flow_break_tokens_.Shrink(0);
}

void NGInlineChildLayoutContext::PropagateParallelFlowBreakToken(
    const NGBreakToken* token) {
  parallel_flow_break_tokens_.push_back(token);
}

}  // namespace blink
