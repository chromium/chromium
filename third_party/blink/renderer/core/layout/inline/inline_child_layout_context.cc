// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/inline_child_layout_context.h"

#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"

namespace blink {

namespace {

struct SameSizeAsInlineChildLayoutContext {
  STACK_ALLOCATED();

 public:
  FragmentItemsBuilder items_builder_;
  std::optional<InlineLayoutStateStack> box_states_;
  std::optional<LayoutUnit> optional_layout_unit;
  void* pointers[5];
  unsigned number;
  HeapVector<Member<const BlockBreakToken>> tokens_;
};

static_assert(
    sizeof(InlineChildLayoutContext) ==
        sizeof(SameSizeAsInlineChildLayoutContext),
    "Only data which can be regenerated from the node, constraints, and break "
    "token are allowed to be placed in this context object.");

// Return true if we're inside a fragmentainer with known block-size (i.e. not
// if we're in an initial column balancing pass, in which case the fragmentainer
// block-size would be unconstrained). This information will be used to
// determine whether it's reasonable to pre-allocate a buffer for all the
// estimated fragment items inside the node.
bool IsBlockFragmented(const BoxFragmentBuilder& fragment_builder) {
  const ConstraintSpace& space = fragment_builder.GetConstraintSpace();
  return space.HasBlockFragmentation() &&
         space.HasKnownFragmentainerBlockSize();
}

}  // namespace

InlineChildLayoutContext::InlineChildLayoutContext(
    const InlineNode& node,
    BoxFragmentBuilder* container_builder,
    LineInfo* line_info)
    : container_builder_(container_builder),
      items_builder_(node,
                     container_builder->GetWritingDirection(),
                     IsBlockFragmented(*container_builder)),
      line_info_(line_info) {
  container_builder->SetItemsBuilder(ItemsBuilder());
}

InlineChildLayoutContext::InlineChildLayoutContext(
    const InlineNode& node,
    BoxFragmentBuilder* container_builder,
    ScoreLineBreakContext* score_line_break_context)
    : container_builder_(container_builder),
      items_builder_(node,
                     container_builder->GetWritingDirection(),
                     IsBlockFragmented(*container_builder)),
      score_line_break_context_(score_line_break_context) {
  container_builder->SetItemsBuilder(ItemsBuilder());
}

InlineChildLayoutContext::~InlineChildLayoutContext() {
  container_builder_->SetItemsBuilder(nullptr);
  parallel_flow_break_tokens_.clear();
}

InlineLayoutStateStack* InlineChildLayoutContext::BoxStatesIfValidForItemIndex(
    const HeapVector<InlineItem>& items,
    unsigned item_index) {
  if (box_states_.has_value() && items_ == &items && item_index_ == item_index)
    return &*box_states_;
  return nullptr;
}

void InlineChildLayoutContext::ClearParallelFlowBreakTokens() {
  parallel_flow_break_tokens_.Shrink(0);
}

void InlineChildLayoutContext::PropagateParallelFlowBreakToken(
    const BreakToken* token) {
  parallel_flow_break_tokens_.push_back(token);
}

}  // namespace blink
