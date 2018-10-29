/*
 * Copyright (C) 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/layout_object_child_list.h"

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/layout/layout_counter.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"

namespace blink {

namespace {

// Invalidate InineItems() in LayoutNGText.
//
// They need to be invalidated when moving across inline formatting context
// (i.e., to a different LayoutBlockFlow.)
void InvalidateInlineItems(LayoutObject* object) {
  if (object->IsInLayoutNGInlineFormattingContext())
    object->SetFirstInlineFragment(nullptr);
  if (object->IsLayoutNGText()) {
    ToLayoutNGText(object)->InvalidateInlineItems();
  } else if (object->IsLayoutInline()) {
    // When moving without |notify_layout_object|, only top-level objects are
    // moved. Ensure to invalidate all LayoutNGText in this inline formatting
    // context.
    for (LayoutObject* curr = object->SlowFirstChild(); curr;
         curr = curr->NextSibling())
      InvalidateInlineItems(curr);
  }
}

}  // namespace

void LayoutObjectChildList::DestroyLeftoverChildren() {
  while (FirstChild()) {
    // List markers are owned by their enclosing list and so don't get destroyed
    // by this container.
    if (FirstChild()->IsListMarker()) {
      FirstChild()->Remove();
      continue;
    }

    // Destroy any anonymous children remaining in the layout tree, as well as
    // implicit (shadow) DOM elements like those used in the engine-based text
    // fields.
    if (FirstChild()->GetNode())
      FirstChild()->GetNode()->SetLayoutObject(nullptr);
    FirstChild()->Destroy();
  }
}

LayoutObject* LayoutObjectChildList::RemoveChildNode(
    LayoutObject* owner,
    LayoutObject* old_child,
    bool notify_layout_object) {
  DCHECK_EQ(old_child->Parent(), owner);
  DCHECK_EQ(this, owner->VirtualChildren());

  if (old_child->IsFloatingOrOutOfFlowPositioned())
    ToLayoutBox(old_child)->RemoveFloatingOrPositionedChildFromBlockLists();

  if (!owner->DocumentBeingDestroyed()) {
    // So that we'll get the appropriate dirty bit set (either that a normal
    // flow child got yanked or that a positioned child got yanked). We also
    // issue paint invalidations, so that the area exposed when the child
    // disappears gets paint invalidated properly.
    if (notify_layout_object && old_child->EverHadLayout())
      old_child->SetNeedsLayoutAndPrefWidthsRecalc(
          LayoutInvalidationReason::kRemovedFromLayout);
    InvalidatePaintOnRemoval(*old_child);
  }

  // If we have a line box wrapper, delete it.
  if (old_child->IsBox())
    ToLayoutBox(old_child)->DeleteLineBoxWrapper();

  if (!owner->DocumentBeingDestroyed()) {
    owner->NotifyOfSubtreeChange();

    if (notify_layout_object) {
      LayoutCounter::LayoutObjectSubtreeWillBeDetached(old_child);
      old_child->WillBeRemovedFromTree();
    } else if (old_child->IsBox() &&
               ToLayoutBox(old_child)->IsOrthogonalWritingModeRoot()) {
      ToLayoutBox(old_child)->UnmarkOrthogonalWritingModeRoot();
    }
  }

  // WARNING: There should be no code running between willBeRemovedFromTree and
  // the actual removal below.
  // This is needed to avoid race conditions where willBeRemovedFromTree would
  // dirty the tree's structure and the code running here would force an
  // untimely rebuilding, leaving |oldChild| dangling.

  if (old_child->PreviousSibling())
    old_child->PreviousSibling()->SetNextSibling(old_child->NextSibling());
  if (old_child->NextSibling())
    old_child->NextSibling()->SetPreviousSibling(old_child->PreviousSibling());

  if (FirstChild() == old_child)
    first_child_ = old_child->NextSibling();
  if (LastChild() == old_child)
    last_child_ = old_child->PreviousSibling();

  old_child->SetPreviousSibling(nullptr);
  old_child->SetNextSibling(nullptr);
  old_child->SetParent(nullptr);

  old_child->RegisterSubtreeChangeListenerOnDescendants(
      old_child->ConsumesSubtreeChangeNotification());

  if (AXObjectCache* cache = owner->GetDocument().ExistingAXObjectCache())
    cache->ChildrenChanged(owner);

  return old_child;
}

DISABLE_CFI_PERF
void LayoutObjectChildList::InsertChildNode(LayoutObject* owner,
                                            LayoutObject* new_child,
                                            LayoutObject* before_child,
                                            bool notify_layout_object) {
  DCHECK(!new_child->Parent());
  DCHECK_EQ(this, owner->VirtualChildren());
  DCHECK(!owner->IsLayoutBlockFlow() ||
         (!new_child->IsTableSection() && !new_child->IsTableRow() &&
          !new_child->IsTableCell()));

  while (before_child && before_child->Parent() &&
         before_child->Parent() != owner)
    before_child = before_child->Parent();

  // This should never happen, but if it does prevent layout tree corruption
  // where child->parent() ends up being owner but
  // child->nextSibling()->parent() is not owner.
  if (before_child && before_child->Parent() != owner) {
    NOTREACHED();
    return;
  }

  new_child->SetParent(owner);

  if (FirstChild() == before_child)
    first_child_ = new_child;

  if (before_child) {
    LayoutObject* previous_sibling = before_child->PreviousSibling();
    if (previous_sibling)
      previous_sibling->SetNextSibling(new_child);
    new_child->SetPreviousSibling(previous_sibling);
    new_child->SetNextSibling(before_child);
    before_child->SetPreviousSibling(new_child);
  } else {
    if (LastChild())
      LastChild()->SetNextSibling(new_child);
    new_child->SetPreviousSibling(LastChild());
    last_child_ = new_child;
  }

  if (!owner->DocumentBeingDestroyed()) {
    if (notify_layout_object) {
      new_child->InsertedIntoTree();
      LayoutCounter::LayoutObjectSubtreeAttached(new_child);
    } else if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
      // |notify_layout_object| is an optimization to skip notifications when
      // moving within the same tree. Inline items need to be invalidated even
      // when moving.
      InvalidateInlineItems(new_child);
    }
  }

  // Propagate the need to notify ancestors down into any
  // child nodes.
  if (owner->HasSubtreeChangeListenerRegistered())
    new_child->RegisterSubtreeChangeListenerOnDescendants(true);

  // If the inserted node is currently marked as needing to notify children then
  // we have to propagate that mark up the tree.
  if (new_child->WasNotifiedOfSubtreeChange())
    owner->NotifyAncestorsOfSubtreeChange();

  // Clear NeedsCollectInlines to ensure the marking doesn't stop on
  // |new_child|.
  new_child->ClearNeedsCollectInlines();

  new_child->SetNeedsLayoutAndPrefWidthsRecalc(
      LayoutInvalidationReason::kAddedToLayout);
  new_child->SetShouldDoFullPaintInvalidation(
      PaintInvalidationReason::kAppeared);
  new_child->AddSubtreePaintPropertyUpdateReason(
      SubtreePaintPropertyUpdateReason::kContainerChainMayChange);
  if (!owner->NormalChildNeedsLayout()) {
    owner->SetChildNeedsLayout();  // We may supply the static position for an
                                   // absolute positioned child.
  } else {
    owner->MarkContainerNeedsCollectInlines();
  }

  if (!owner->DocumentBeingDestroyed())
    owner->NotifyOfSubtreeChange();

  if (AXObjectCache* cache = owner->GetDocument().ExistingAXObjectCache())
    cache->ChildrenChanged(owner);
}

void LayoutObjectChildList::InvalidatePaintOnRemoval(LayoutObject& old_child) {
  if (!old_child.IsRooted())
    return;
  if (old_child.IsBody() && old_child.View())
    old_child.View()->SetShouldDoFullPaintInvalidation();
  ObjectPaintInvalidator paint_invalidator(old_child);
  paint_invalidator.SlowSetPaintingLayerNeedsRepaint();
}

}  // namespace blink
