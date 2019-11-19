// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LEGACY_LAYOUT_TREE_WALKING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LEGACY_LAYOUT_TREE_WALKING_H_

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"

// We still have the legacy layout tree structure, which means that a multicol
// container LayoutBlockFlow will consist of a LayoutFlowThread child, followed
// by zero or more siblings of type LayoutMultiColumnSet and/or
// LayoutMultiColumnSpannerPlaceholder. NG needs to skip these special
// objects. The actual content is inside the flow thread. There are similar
// complications for fieldset / legend.

namespace blink {

// Return the layout object that should be the first child NGLayoutInputNode of
// |parent|. Normally this will just be the first layout object child, but there
// are certain layout objects that should be skipped for NG.
inline LayoutObject* GetLayoutObjectForFirstChildNode(LayoutBlock* parent) {
  LayoutObject* child = parent->FirstChild();
  if (!child)
    return nullptr;
  if (UNLIKELY(child->IsLayoutFlowThread()))
    return To<LayoutBlockFlow>(child)->FirstChild();
  // The rendered legend is a child of the anonymous wrapper inside the fieldset
  // container. If we find it, skip it. As far as NG is concerned, the rendered
  // legend is a child of the fieldset container.
  if (UNLIKELY(child->IsRenderedLegend()))
    return child->NextSibling();
  return child;
}

// Return the layout object that should be the parent NGLayoutInputNode of
// |object|. Normally this will just be the parent layout object, but there
// are certain layout objects that should be skipped for NG.
inline LayoutObject* GetLayoutObjectForParentNode(LayoutObject* object) {
  // First check that we're not walking where we shouldn't be walking.
  DCHECK(!object->IsLayoutFlowThread());
  DCHECK(!object->IsLayoutMultiColumnSet());
  DCHECK(!object->IsLayoutMultiColumnSpannerPlaceholder());

  LayoutObject* parent = object->Parent();
  if (UNLIKELY(!parent))
    return nullptr;

  // The parent of the rendered legend is the fieldset container, as far as NG
  // is concerned. Skip the anonymous wrapper in-between.
  if (UNLIKELY(object->IsRenderedLegend()))
    return parent->Parent();

  if (UNLIKELY(parent->IsLayoutFlowThread()))
    return parent->Parent();
  return parent;
}

// Return the layout object that should be the sibling NGLayoutInputNode of
// |object|. Normally this will just be the next sibling layout object, but
// there are certain layout objects that should be skipped for NG.
inline LayoutObject* GetLayoutObjectForNextSiblingNode(LayoutObject* object) {
  // We don't expect to walk the layout tree starting at the rendered legend,
  // and we'll skip over it if we find it. The renderered legend will be handled
  // by a special algorithm, and should be invisible among siblings.
  DCHECK(!object->IsRenderedLegend());
  LayoutObject* next = object->NextSibling();
  if (!next)
    return nullptr;
  if (UNLIKELY(next->IsRenderedLegend()))
    return next->NextSibling();
  return next;
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
  return containing_block && containing_block->IsLayoutNGMixin();
}

// Return true if the layout object is a LayoutNG object that is managed by the
// LayoutNG engine (i.e. its containing block is a LayoutNG object as well).
inline bool IsManagedByLayoutNG(const LayoutObject& object) {
  if (!object.IsLayoutNGMixin())
    return false;
  const auto* containing_block = object.ContainingBlock();
  if (UNLIKELY(!containing_block))
    return false;
  return IsLayoutNGContainingBlock(containing_block);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LEGACY_LAYOUT_TREE_WALKING_H_
