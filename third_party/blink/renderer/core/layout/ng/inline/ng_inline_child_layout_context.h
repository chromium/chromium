// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_CHILD_LAYOUT_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_CHILD_LAYOUT_CONTEXT_H_

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
  NGInlineChildLayoutContext();
  ~NGInlineChildLayoutContext();

  NGFragmentItemsBuilder* ItemsBuilder() { return items_builder_; }
  void SetItemsBuilder(NGFragmentItemsBuilder* builder) {
    DCHECK(!items_builder_ || !builder);
    items_builder_ = builder;
    if (builder)
      builder->AddLogicalLineItemsPool(&logical_line_items_);
  }

  // Returns an instance of |NGLogicalLineItems|. This is reused when laying out
  // the next line.
  NGLogicalLineItems* LogicalLineItems() { return &logical_line_items_; }

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
      const Vector<NGInlineItem>& items,
      unsigned item_index);
  void SetItemIndex(const Vector<NGInlineItem>& items, unsigned item_index) {
    items_ = &items;
    item_index_ = item_index;
  }

  const Vector<scoped_refptr<const NGBlockBreakToken>>& PropagatedBreakTokens()
      const {
    return propagated_float_break_tokens_;
  }
  void ClearPropagatedBreakTokens();
  void PropagateBreakToken(scoped_refptr<const NGBlockBreakToken>);

 private:
  // TODO(kojii): Probably better to own |NGInlineChildLayoutContext|. While we
  // transit, allocating separately is easier.
  NGFragmentItemsBuilder* items_builder_ = nullptr;

  NGLogicalLineItems logical_line_items_;

  base::Optional<NGInlineLayoutStateStack> box_states_;

  // The items and its index this context is set up for.
  const Vector<NGInlineItem>* items_ = nullptr;
  unsigned item_index_ = 0;

  Vector<scoped_refptr<const NGBlockBreakToken>> propagated_float_break_tokens_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_CHILD_LAYOUT_CONTEXT_H_
