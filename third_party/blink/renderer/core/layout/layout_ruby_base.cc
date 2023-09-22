// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_ruby_base.h"

namespace blink {

LayoutRubyBase::LayoutRubyBase() : LayoutNGBlockFlow(nullptr) {
  SetInline(false);
}

LayoutRubyBase::~LayoutRubyBase() = default;

bool LayoutRubyBase::IsOfType(LayoutObjectType type) const {
  NOT_DESTROYED();
  return type == kLayoutObjectRubyBase || LayoutBlockFlow::IsOfType(type);
}

bool LayoutRubyBase::IsChildAllowed(LayoutObject*, const ComputedStyle&) const {
  NOT_DESTROYED();
  NOTREACHED();  // Because LayoutRubyColumn manages child types.
  return true;
}

void LayoutRubyBase::MoveChildren(LayoutRubyBase& to_base,
                                  LayoutObject* before_child) {
  NOT_DESTROYED();

  if (before_child && before_child->Parent() != this) {
    before_child = SplitAnonymousBoxesAroundChild(before_child);
  }

  if (ChildrenInline()) {
    MoveInlineChildrenTo(to_base, before_child);
  } else {
    MoveBlockChildrenTo(to_base, before_child);
  }

  SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kUnknown);
  to_base.SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kUnknown);
}

void LayoutRubyBase::MoveInlineChildrenTo(LayoutRubyBase& to_base,
                                          LayoutObject* before_child) {
  NOT_DESTROYED();
  DCHECK(ChildrenInline());

  if (!FirstChild()) {
    return;
  }

  LayoutBlock* to_block;
  if (to_base.ChildrenInline()) {
    // The standard and easy case: move the children into the target base
    to_block = &to_base;
  } else {
    // We need to wrap the inline objects into an anonymous block.
    // If toBase has a suitable block, we re-use it, otherwise create a new one.
    LayoutObject* last_child = to_base.LastChild();
    if (last_child && last_child->IsAnonymousBlock() &&
        last_child->ChildrenInline()) {
      to_block = To<LayoutBlock>(last_child);
    } else {
      to_block = to_base.CreateAnonymousBlock();
      to_base.Children()->AppendChildNode(&to_base, to_block);
    }
  }
  // Move our inline children into the target block we determined above.
  MoveChildrenTo(to_block, FirstChild(), before_child);
}

void LayoutRubyBase::MoveBlockChildrenTo(LayoutRubyBase& to_base,
                                         LayoutObject* before_child) {
  NOT_DESTROYED();
  DCHECK(!ChildrenInline());

  if (!FirstChild()) {
    return;
  }

  if (to_base.ChildrenInline()) {
    to_base.MakeChildrenNonInline();
  }

  // If an anonymous block would be put next to another such block, then merge
  // those.
  LayoutObject* first_child_here = FirstChild();
  LayoutObject* last_child_there = to_base.LastChild();
  if (first_child_here->IsAnonymousBlock() &&
      first_child_here->ChildrenInline() && last_child_there &&
      last_child_there->IsAnonymousBlock() &&
      last_child_there->ChildrenInline()) {
    auto* anon_block_here = To<LayoutBlockFlow>(first_child_here);
    auto* anon_block_there = To<LayoutBlockFlow>(last_child_there);
    anon_block_here->MoveAllChildrenTo(anon_block_there,
                                       anon_block_there->Children());
    anon_block_here->Destroy();
  }
  // Move all remaining children normally. If moving all children, include our
  // float list.
  if (!before_child) {
    bool full_remove_insert = to_base.HasLayer() || HasLayer();
    // TODO(kojii): |this| is |!ChildrenInline()| when we enter this function,
    // but it may turn to |ChildrenInline()| when |anon_block_here| is destroyed
    // above. Probably the correct fix is to do it earlier and switch to
    // |MoveInlineChildren()| if this happens. For the short term safe fix,
    // using |full_remove_insert| can prevent inconsistent LayoutObject tree
    // that leads to CHECK failures.
    full_remove_insert |= ChildrenInline();
    MoveAllChildrenIncludingFloatsTo(&to_base, full_remove_insert);
  } else {
    MoveChildrenTo(&to_base, FirstChild(), before_child);
  }
}

}  // namespace blink
