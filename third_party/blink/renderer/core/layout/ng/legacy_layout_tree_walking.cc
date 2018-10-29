// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/legacy_layout_tree_walking.h"

#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"

namespace blink {

// We still have the legacy layout tree structure, which means that a multicol
// container LayoutBlockFlow will consist of a LayoutFlowThread child, followed
// by zero or more siblings of type LayoutMultiColumnSet and/or
// LayoutMultiColumnSpannerPlaceholder. NG needs to skip these special
// objects. The actual content is inside the flow thread.

LayoutObject* GetLayoutObjectForFirstChildNode(LayoutBlock* parent) {
  LayoutObject* child = parent->FirstChild();
  if (!child)
    return nullptr;
  if (child->IsLayoutFlowThread())
    return ToLayoutBlockFlow(child)->FirstChild();
  // The rendered legend is a child of the anonymous wrapper inside the fieldset
  // container. If we find it, skip it. As far as NG is concerned, the rendered
  // legend is a child of the fieldset container.
  if (UNLIKELY(child->IsRenderedLegend()))
    return child->NextSibling();
  return child;
}

LayoutObject* GetLayoutObjectForParentNode(LayoutObject* object) {
  // First check that we're not walking where we shouldn't be walking.
  DCHECK(!object->IsLayoutFlowThread());
  DCHECK(!object->IsLayoutMultiColumnSet());
  DCHECK(!object->IsLayoutMultiColumnSpannerPlaceholder());

  LayoutObject* parent = object->Parent();
  if (!parent)
    return nullptr;

  // The parent of the rendered legend is the fieldset container, as far as NG
  // is concerned. Skip the anonymous wrapper in-between.
  if (UNLIKELY(object->IsRenderedLegend()))
    return parent->Parent();

  if (parent->IsLayoutFlowThread())
    return parent->Parent();
  return parent;
}

LayoutObject* GetLayoutObjectForNextSiblingNode(LayoutObject* object) {
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

bool AreNGBlockFlowChildrenInline(const LayoutBlock* block) {
  if (block->ChildrenInline())
    return true;
  if (const auto* first_child = block->FirstChild()) {
    if (first_child->IsLayoutFlowThread())
      return first_child->ChildrenInline();
  }
  return false;
}

bool IsManagedByLayoutNG(const LayoutObject& object) {
  if (!object.IsLayoutNGMixin() && !object.IsLayoutNGFlexibleBox())
    return false;
  const auto* containing_block = object.ContainingBlock();
  if (!containing_block)
    return false;
  return IsLayoutNGContainingBlock(containing_block);
}

bool IsLayoutNGContainingBlock(const LayoutBlock* containing_block) {
  if (containing_block->IsLayoutFlowThread())
    containing_block = containing_block->ContainingBlock();
  return containing_block && (containing_block->IsLayoutNGMixin() ||
                              containing_block->IsLayoutNGFlexibleBox());
}

}  // namespace blink
