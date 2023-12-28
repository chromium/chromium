// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/layout_subtree_root_list.h"

#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

void LayoutSubtreeRootList::ClearAndMarkContainingBlocksForLayout() {
  for (const auto& iter : Unordered())
    iter->MarkContainerChainForLayout(false);
  Clear();
}

void LayoutSubtreeRootList::CountObjectsNeedingLayoutInRoot(
    const LayoutObject* object,
    unsigned& needs_layout_objects,
    unsigned& total_objects) {
  for (const LayoutObject* o = object; o;) {
    ++total_objects;
    bool display_locked = o->ChildLayoutBlockedByDisplayLock();
    if (o->SelfNeedsFullLayout() || (!display_locked && o->NeedsLayout())) {
      ++needs_layout_objects;
    }

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
  for (const auto& root : Unordered())
    CountObjectsNeedingLayoutInRoot(root, needs_layout_objects, total_objects);
}

}  // namespace blink
