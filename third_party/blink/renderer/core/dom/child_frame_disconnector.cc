// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/child_frame_disconnector.h"

#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

#if DCHECK_IS_ON()
static unsigned CheckConnectedSubframeCountIsConsistent(Node&);
#endif

void ChildFrameDisconnector::Disconnect(DisconnectPolicy policy) {
#if DCHECK_IS_ON()
  CheckConnectedSubframeCountIsConsistent(Root());
#endif

  if (!Root().ConnectedSubframeCount())
    return;

  if (policy == kRootAndDescendants) {
    CollectFrameOwners(Root());
  } else {
    for (Node* child = Root().firstChild(); child; child = child->nextSibling())
      CollectFrameOwners(*child);
  }

  DisconnectCollectedFrameOwners();
}

void ChildFrameDisconnector::CollectFrameOwners(Node& root) {
  if (!root.ConnectedSubframeCount())
    return;

  if (auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(root))
    frame_owners_.push_back(frame_owner);

  for (Node* child = root.firstChild(); child; child = child->nextSibling())
    CollectFrameOwners(*child);

  if (ShadowRoot* shadow_root = root.GetShadowRoot())
    CollectFrameOwners(*shadow_root);
}

void ChildFrameDisconnector::DisconnectCollectedFrameOwners() {
  // Must disable frame loading in the subtree so an unload handler cannot
  // insert more frames and create loaded frames in detached subtrees.
  SubframeLoadingDisabler disabler(Root());

  for (unsigned i = 0; i < frame_owners_.size(); ++i) {
    HTMLFrameOwnerElement* owner = frame_owners_[i].Get();
    // Don't need to traverse up the tree for the first owner since no
    // script could have moved it.
    if (!i || Root().IsShadowIncludingInclusiveAncestorOf(*owner))
      owner->DisconnectContentFrame();
  }
}

#if DCHECK_IS_ON()
static unsigned CheckConnectedSubframeCountIsConsistent(Node& node) {
  unsigned count = 0;
  if (auto* element = DynamicTo<Element>(node)) {
    auto* frame_owner_element = DynamicTo<HTMLFrameOwnerElement>(node);
    if (frame_owner_element && frame_owner_element->ContentFrame())
      count++;

    if (ShadowRoot* root = element->GetShadowRoot())
      count += CheckConnectedSubframeCountIsConsistent(*root);
  }

  for (Node* child = node.firstChild(); child; child = child->nextSibling())
    count += CheckConnectedSubframeCountIsConsistent(*child);

  // If we undercount there's possibly a security bug since we'd leave frames
  // in subtrees outside the document.
  DCHECK_GE(node.ConnectedSubframeCount(), count);

  // If we overcount it's safe, but not optimal because it means we'll traverse
  // through the document in ChildFrameDisconnector looking for frames that have
  // already been disconnected.
  DCHECK_EQ(node.ConnectedSubframeCount(), count);

  return count;
}
#endif

}  // namespace blink
