// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_CHILD_LAYOUT_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_CHILD_LAYOUT_CONTEXT_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items_builder.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_box_state.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_logical_line_item.h"

namespace blink {

class NGInlineItem;

// A context object given to layout. The same instance should be given to
// children of a parent node, but layout algorithm should be prepared to be
// given a new instance when yield or fragmentation occur.
//
// Because this context is in initial state for when fragmentation occurs and
// some other cases, do not add things that are too expensive to rebuild.
class CORE_EXPORT NGInlineChildLayoutContext {
  STACK_ALLOCATED();

 public:
  NGInlineChildLayoutContext(const NGInlineNode& node,
                             NGBoxFragmentBuilder* container_builder);
  ~NGInlineChildLayoutContext();

  NGFragmentItemsBuilder* ItemsBuilder() { return &items_builder_; }

  // Acquire/release temporary |NGLogicalLineItems|, used for a short period of
  // time, but needed multiple times in a context.
  NGLogicalLineItems& AcquireTempLogicalLineItems();
  void ReleaseTempLogicalLineItems(NGLogicalLineItems&);

  // Returns the NGInlineLayoutStateStack in this context.
  bool HasBoxStates() const { return box_states_.has_value(); }
  NGInlineLayoutStateStack* BoxStates() { return &*box_states_; }
  NGInlineLayoutStateStack* ResetBoxStates() { return &box_states_.emplace(); }

  // Returns the box states in this context if it exists and it can be used to
  // create a line starting from |items[item_index}|, otherwise returns nullptr.
  //
  // To determine this, callers must call |SetItemIndex| to set the end of the
  // current line.
  NGInlineLayoutStateStack* BoxStatesIfValidForItemIndex(
      const HeapVector<NGInlineItem>& items,
      unsigned item_index);
  void SetItemIndex(const HeapVector<NGInlineItem>& items,
                    unsigned item_index) {
    items_ = &items;
    item_index_ = item_index;
  }

  const HeapVector<Member<const NGBlockBreakToken>>& PropagatedBreakTokens()
      const {
    return propagated_float_break_tokens_;
  }
  void ClearPropagatedBreakTokens();
  void PropagateBreakToken(const NGBlockBreakToken*);

  const absl::optional<LayoutUnit>& BalancedAvailableWidth() const {
    return balanced_available_width_;
  }
  void SetBalancedAvailableWidth(absl::optional<LayoutUnit> value) {
    balanced_available_width_ = value;
  }

 private:
  NGBoxFragmentBuilder* container_builder_ = nullptr;
  NGFragmentItemsBuilder items_builder_;

  NGLogicalLineItems* temp_logical_line_items_ = nullptr;

  absl::optional<NGInlineLayoutStateStack> box_states_;

  // The items and its index this context is set up for.
  const HeapVector<NGInlineItem>* items_ = nullptr;
  unsigned item_index_ = 0;

  HeapVector<Member<const NGBlockBreakToken>> propagated_float_break_tokens_;

  // Used by `NGParagraphLineBreaker`.
  absl::optional<LayoutUnit> balanced_available_width_;
};

inline NGLogicalLineItems&
NGInlineChildLayoutContext::AcquireTempLogicalLineItems() {
  if (NGLogicalLineItems* line_items = temp_logical_line_items_) {
    temp_logical_line_items_ = nullptr;
    DCHECK_EQ(line_items->size(), 0u);
    return *line_items;
  }
  return *MakeGarbageCollected<NGLogicalLineItems>();
}

inline void NGInlineChildLayoutContext::ReleaseTempLogicalLineItems(
    NGLogicalLineItems& line_items) {
  DCHECK(&line_items);
  line_items.clear();
  temp_logical_line_items_ = &line_items;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_CHILD_LAYOUT_CONTEXT_H_
