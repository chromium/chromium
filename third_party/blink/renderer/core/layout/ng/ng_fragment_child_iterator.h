// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FRAGMENT_CHILD_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FRAGMENT_CHILD_ITERATOR_H_

#include "base/containers/span.h"
#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/ng_link.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

class LayoutObject;
class NGBlockBreakToken;

// Iterator for children of a box fragment. Supports fragment items and break
// tokens. To advance to the next sibling, call |Advance()|. To descend into
// children of the current child, call |Descend()|.
//
// Using this class requires LayoutNGFragmentItem to be enabled. While fragment
// items are in a flat list representing the contents of an inline formatting
// context, the iterator will to a certain extent restore the object hierarchy,
// so that we can calculate the global offset of children of a relatively
// positioned inline correctly.
class CORE_EXPORT NGFragmentChildIterator {
  STACK_ALLOCATED();

 public:
  explicit NGFragmentChildIterator(
      const NGPhysicalBoxFragment& parent,
      const NGBlockBreakToken* parent_break_token = nullptr);

  // Create a child iterator for the current child.
  NGFragmentChildIterator Descend() const;

  // Move to the next sibling. Return false if there's no next sibling. Once
  // false is returned, this object is in an unusable state, with the exception
  // that calling IsAtEnd() is allowed.
  bool Advance() {
    if (current_.cursor_)
      return AdvanceWithCursor();
    return AdvanceChildFragment();
  }

  bool IsAtEnd() {
    if (current_.cursor_)
      return !*current_.cursor_;
    DCHECK(parent_fragment_);
    const auto children = parent_fragment_->Children();
    return child_fragment_idx_ >= children.size();
  }

  class Current {
    friend class NGFragmentChildIterator;

   public:
    // Return the current NGLink. Note that its offset is relative to the inline
    // formatting context root, if the fragment / item participates in one.
    const NGLink& Link() const { return link_; }

    const NGPhysicalBoxFragment* BoxFragment() const {
      return To<NGPhysicalBoxFragment>(link_.fragment);
    }
    const NGFragmentItem* FragmentItem() const {
      if (!cursor_)
        return nullptr;
      return cursor_->CurrentItem();
    }

    // Get the incoming break token for the current child, i.e. the context at
    // which layout of this child's node was resumed. Note that for text and
    // non-atomic inlines this will be the incoming block break token to the
    // inline formatting context root. For monolithic content, no break token
    // will be returned (since such content isn't considered to participate in a
    // fragmentation context).
    const NGBlockBreakToken* BlockBreakToken() const {
      if (LIKELY(!block_break_token_))
        return nullptr;
      if (link_.fragment) {
        // Don't pass the break token into monolithic content.
        if (link_.fragment->IsMonolithic())
          return nullptr;
        // If the break token we've found is from a fragmentainer, it's only to
        // be used by a subsequent fragmentainer. Other fragment types (such as
        // column spanners) need to ignore it.
        if (break_token_for_fragmentainer_only_ &&
            !link_.fragment->IsFragmentainerBox())
          return nullptr;
      }
      return block_break_token_;
    }

    const LayoutObject* GetLayoutObject() const {
      if (const NGFragmentItem* item = FragmentItem())
        return item->GetLayoutObject();
      return BoxFragment()->GetLayoutObject();
    }

   private:
    NGLink link_;
    base::Optional<NGInlineCursor> cursor_;
    const NGBlockBreakToken* block_break_token_ = nullptr;
    bool break_token_for_fragmentainer_only_ = false;
  };

  const Current& GetCurrent() const { return current_; }
  const Current& operator*() const { return current_; }
  const Current* operator->() const { return &current_; }

 private:
  NGFragmentChildIterator(
      const NGInlineCursor& parent,
      const NGBlockBreakToken* parent_break_token,
      base::span<const NGBreakToken* const> child_break_tokens);

  bool AdvanceChildFragment();
  void UpdateSelfFromFragment(
      const NGPhysicalBoxFragment* previous_fragment = nullptr);

  bool AdvanceWithCursor();
  void UpdateSelfFromCursor();
  void SkipToBoxFragment();
  void SkipToBlockBreakToken();

  const NGPhysicalBoxFragment* parent_fragment_ = nullptr;
  const NGBlockBreakToken* parent_break_token_ = nullptr;
  Current current_;
  base::span<const NGBreakToken* const> child_break_tokens_;
  wtf_size_t child_fragment_idx_ = 0;
  wtf_size_t child_break_token_idx_ = 0;
  bool is_fragmentation_context_root_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FRAGMENT_CHILD_ITERATOR_H_
