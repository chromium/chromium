// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LEGACY_LAYOUT_TREE_WALKING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LEGACY_LAYOUT_TREE_WALKING_H_

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"

// We still have the legacy layout tree structure, which means that a multicol
// container LayoutBlockFlow will consist of a LayoutFlowThread child, followed
// by zero or more siblings of type LayoutMultiColumnSet and/or
// LayoutMultiColumnSpannerPlaceholder. NG needs to skip these special
// objects. The actual content is inside the flow thread.

namespace blink {

// Return the layout object that should be the first child NGLayoutInputNode of
// |parent|. Normally this will just be the first layout object child, but there
// are certain layout objects that should be skipped for NG.
inline LayoutObject* GetLayoutObjectForFirstChildNode(LayoutBlock* parent) {
  LayoutObject* child = parent->FirstChild();
  if (!child)
    return nullptr;
  if (UNLIKELY(child->IsLayoutFlowThread()))
    child = To<LayoutBlockFlow>(child)->FirstChild();
  return child;
}

// Return the layout object that should be the parent NGLayoutInputNode of
// |object|. Normally this will just be the parent layout object, but there
// are certain layout objects that should be skipped for NG.
//
// |Type| should be either "LayoutObject*" or "const LayoutObject*", and may be
// deduced automatically at the call site, based on the type of |object| (unless
// it's a subclass of LayoutObject rather than LayoutObject itself).
template <typename Type>
inline Type GetLayoutObjectForParentNode(Type object) {
  // First check that we're not walking where we shouldn't be walking.
  DCHECK(!object->IsLayoutFlowThread());
  DCHECK(!object->IsLayoutMultiColumnSet());
  DCHECK(!object->IsLayoutMultiColumnSpannerPlaceholder());

  Type parent = object->Parent();
  if (UNLIKELY(!parent))
    return nullptr;

  if (UNLIKELY(parent->IsLayoutFlowThread()))
    return parent->Parent();
  return parent;
}

// Return true if the NGLayoutInputNode children of the NGLayoutInputNode
// established by |block| will be inline; see LayoutObject::ChildrenInline().
inline bool AreNGBlockFlowChildrenInline(const LayoutBlock* block) {
  if (block->ChildrenInline())
    return true;
  if (const auto* first_child = block->FirstChild()) {
    if (UNLIKELY(first_child->IsLayoutFlowThread()))
      return first_child->ChildrenInline();
  }
  return false;
}

// Return true if the block is of NG type, or if it's a block invisible to
// LayoutNG and it has an NG containg block. False if it's hosted by the legacy
// layout engine.
inline bool IsLayoutNGContainingBlock(const LayoutBlock* containing_block) {
  if (UNLIKELY(containing_block->IsLayoutFlowThread()))
    containing_block = containing_block->ContainingBlock();
  return containing_block && containing_block->IsLayoutNGObject();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LEGACY_LAYOUT_TREE_WALKING_H_
