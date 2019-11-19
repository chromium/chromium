/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/layout_ruby_base.h"

namespace blink {

LayoutRubyBase::LayoutRubyBase() : LayoutBlockFlow(nullptr) {
  SetInline(false);
}

LayoutRubyBase::~LayoutRubyBase() = default;

LayoutRubyBase* LayoutRubyBase::CreateAnonymous(Document* document) {
  LayoutRubyBase* layout_object = new LayoutRubyBase();
  layout_object->SetDocumentForAnonymous(document);
  return layout_object;
}

bool LayoutRubyBase::IsChildAllowed(LayoutObject* child,
                                    const ComputedStyle&) const {
  return child->IsInline();
}

void LayoutRubyBase::MoveChildren(LayoutRubyBase* to_base,
                                  LayoutObject* before_child) {
  // This function removes all children that are before (!) beforeChild
  // and appends them to toBase.
  DCHECK(to_base);
  // Callers should have handled the percent height descendant map.
  DCHECK(!HasPercentHeightDescendants());

  if (before_child && before_child->Parent() != this)
    before_child = SplitAnonymousBoxesAroundChild(before_child);

  if (ChildrenInline())
    MoveInlineChildren(to_base, before_child);
  else
    MoveBlockChildren(to_base, before_child);

  SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kUnknown);
  to_base->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kUnknown);
}

void LayoutRubyBase::MoveInlineChildren(LayoutRubyBase* to_base,
                                        LayoutObject* before_child) {
  DCHECK(ChildrenInline());
  DCHECK(to_base);

  if (!FirstChild())
    return;

  LayoutBlock* to_block;
  if (to_base->ChildrenInline()) {
    // The standard and easy case: move the children into the target base
    to_block = to_base;
  } else {
    // We need to wrap the inline objects into an anonymous block.
    // If toBase has a suitable block, we re-use it, otherwise create a new one.
    LayoutObject* last_child = to_base->LastChild();
    if (last_child && last_child->IsAnonymousBlock() &&
        last_child->ChildrenInline()) {
      to_block = To<LayoutBlock>(last_child);
    } else {
      to_block = to_base->CreateAnonymousBlock();
      to_base->Children()->AppendChildNode(to_base, to_block);
    }
  }
  // Move our inline children into the target block we determined above.
  MoveChildrenTo(to_block, FirstChild(), before_child);
}

void LayoutRubyBase::MoveBlockChildren(LayoutRubyBase* to_base,
                                       LayoutObject* before_child) {
  DCHECK(!ChildrenInline());
  DCHECK(to_base);

  if (!FirstChild())
    return;

  if (to_base->ChildrenInline())
    to_base->MakeChildrenNonInline();

  // If an anonymous block would be put next to another such block, then merge
  // those.
  LayoutObject* first_child_here = FirstChild();
  LayoutObject* last_child_there = to_base->LastChild();
  if (first_child_here->IsAnonymousBlock() &&
      first_child_here->ChildrenInline() && last_child_there &&
      last_child_there->IsAnonymousBlock() &&
      last_child_there->ChildrenInline()) {
    auto* anon_block_here = To<LayoutBlockFlow>(first_child_here);
    auto* anon_block_there = To<LayoutBlockFlow>(last_child_there);
    anon_block_here->MoveAllChildrenTo(anon_block_there,
                                       anon_block_there->Children());
    anon_block_here->DeleteLineBoxTree();
    anon_block_here->Destroy();
  }
  // Move all remaining children normally. If moving all children, include our
  // float list.
  if (!before_child) {
    bool full_remove_insert = to_base->HasLayer() || HasLayer();
    // TODO(kojii): |this| is |!ChildrenInline()| when we enter this function,
    // but it may turn to |ChildrenInline()| when |anon_block_here| is destroyed
    // above. Probably the correct fix is to do it earlier and switch to
    // |MoveInlineChildren()| if this happens. For the short term safe fix,
    // using |full_remove_insert| can prevent inconsistent LayoutObject tree
    // that leads to CHECK failures.
    full_remove_insert |= ChildrenInline();
    MoveAllChildrenIncludingFloatsTo(to_base, full_remove_insert);
  } else {
    MoveChildrenTo(to_base, FirstChild(), before_child);
    RemoveFloatingObjectsFromDescendants();
  }
}

ETextAlign LayoutRubyBase::TextAlignmentForLine(
    bool /* endsWithSoftBreak */) const {
  return ETextAlign::kJustify;
}

void LayoutRubyBase::AdjustInlineDirectionLineBounds(
    unsigned expansion_opportunity_count,
    LayoutUnit& logical_left,
    LayoutUnit& logical_width) const {
  int max_preferred_logical_width = MaxPreferredLogicalWidth().ToInt();
  if (max_preferred_logical_width >= logical_width)
    return;

  unsigned max_count = static_cast<unsigned>(LayoutUnit::Max().Floor());
  if (expansion_opportunity_count > max_count)
    expansion_opportunity_count = max_count;

  // Inset the ruby base by half the inter-ideograph expansion amount.
  LayoutUnit inset = (logical_width - max_preferred_logical_width) /
                     (expansion_opportunity_count + 1);

  logical_left += inset / 2;
  logical_width -= inset;
}

}  // namespace blink
