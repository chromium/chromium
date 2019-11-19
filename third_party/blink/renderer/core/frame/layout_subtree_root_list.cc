// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/layout_subtree_root_list.h"

#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

void LayoutSubtreeRootList::ClearAndMarkContainingBlocksForLayout() {
  for (auto* const iter : Unordered())
    iter->MarkContainerChainForLayout(false);
  Clear();
}

LayoutObject* LayoutSubtreeRootList::RandomRoot() {
  DCHECK(!IsEmpty());
  return *Unordered().begin();
}

void LayoutSubtreeRootList::CountObjectsNeedingLayoutInRoot(
    const LayoutObject* object,
    unsigned& needs_layout_objects,
    unsigned& total_objects) {
  for (const LayoutObject* o = object; o;) {
    ++total_objects;
    bool display_locked =
        o->LayoutBlockedByDisplayLock(DisplayLockLifecycleTarget::kChildren);
    if (o->SelfNeedsLayout() || (!display_locked && o->NeedsLayout()))
      ++needs_layout_objects;

    if (display_locked)
      o = o->NextInPreOrderAfterChildren(object);
    else
      o = o->NextInPreOrder(object);
  }
}

void LayoutSubtreeRootList::CountObjectsNeedingLayout(
    unsigned& needs_layout_objects,
    unsigned& total_objects) {
  // TODO(leviw): This will double-count nested roots crbug.com/509141
  for (auto* const root : Unordered())
    CountObjectsNeedingLayoutInRoot(root, needs_layout_objects, total_objects);
}

}  // namespace blink
