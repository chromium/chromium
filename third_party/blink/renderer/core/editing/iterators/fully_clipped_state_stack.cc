// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/iterators/fully_clipped_state_stack.h"

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"

namespace blink {

namespace {

inline bool FullyClipsContents(const Node* node) {
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object || !layout_object->IsBox() ||
      !layout_object->IsScrollContainer() || IsA<LayoutView>(layout_object))
    return false;
  return To<LayoutBox>(layout_object)->Size().IsEmpty();
}

inline bool IgnoresContainerClip(const Node* node) {
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object || layout_object->IsText())
    return false;
  return layout_object->Style()->HasOutOfFlowPosition();
}

template <typename Strategy>
unsigned DepthCrossingShadowBoundaries(const Node& node) {
  unsigned depth = 0;
  for (ContainerNode* parent = ParentCrossingShadowBoundaries<Strategy>(node);
       parent; parent = ParentCrossingShadowBoundaries<Strategy>(*parent))
    ++depth;
  return depth;
}

}  // namespace

template <typename Strategy>
FullyClippedStateStackAlgorithm<Strategy>::FullyClippedStateStackAlgorithm() =
    default;

template <typename Strategy>
FullyClippedStateStackAlgorithm<Strategy>::~FullyClippedStateStackAlgorithm() =
    default;

template <typename Strategy>
void FullyClippedStateStackAlgorithm<Strategy>::PushFullyClippedState(
    const Node* node) {
  DCHECK_EQ(size(), DepthCrossingShadowBoundaries<Strategy>(*node));

  // FIXME: fully_clipped_stack_ was added in response to
  // <https://bugs.webkit.org/show_bug.cgi?id=26364> ("Search can find text
  // that's hidden by overflow:hidden"), but the logic here will not work
  // correctly if a shadow tree redistributes nodes. fully_clipped_stack_ relies
  // on the assumption that DOM node hierarchy matches the layout tree, which is
  // not necessarily true if there happens to be shadow DOM distribution or
  // other mechanics that shuffle around the layout objects regardless of node
  // tree hierarchy (like CSS flexbox).
  //
  // A more appropriate way to handle this situation is to detect
  // overflow:hidden blocks by using only layout primitives, not with DOM
  // primitives.

  // Push true if this node full clips its contents, or if a parent already has
  // fully
  // clipped and this is not a node that ignores its container's clip.
  Push(FullyClipsContents(node) || (Top() && !IgnoresContainerClip(node)));
}

template <typename Strategy>
void FullyClippedStateStackAlgorithm<Strategy>::SetUpFullyClippedStack(
    const Node* node) {
  // Put the nodes in a vector so we can iterate in reverse order.
  HeapVector<Member<ContainerNode>, 100> ancestry;
  for (ContainerNode* parent = ParentCrossingShadowBoundaries<Strategy>(*node);
       parent; parent = ParentCrossingShadowBoundaries<Strategy>(*parent))
    ancestry.push_back(parent);

  // Call pushFullyClippedState on each node starting with the earliest
  // ancestor.
  wtf_size_t ancestry_size = ancestry.size();
  for (wtf_size_t i = 0; i < ancestry_size; ++i)
    PushFullyClippedState(ancestry[ancestry_size - i - 1]);
  PushFullyClippedState(node);

  DCHECK_EQ(size(), 1 + DepthCrossingShadowBoundaries<Strategy>(*node));
}

template class CORE_TEMPLATE_EXPORT
    FullyClippedStateStackAlgorithm<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT
    FullyClippedStateStackAlgorithm<EditingInFlatTreeStrategy>;

}  // namespace blink
