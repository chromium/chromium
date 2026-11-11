// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CHILD_FRAME_DISCONNECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CHILD_FRAME_DISCONNECTOR_H_

#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Document;
class HTMLFrameOwnerElement;
class Node;

class ChildFrameDisconnector {
  STACK_ALLOCATED();

 public:
  enum DisconnectPolicy { kRootAndDescendants, kDescendantsOnly };
  enum DisconnectReason { kDisconnectParent, kDisconnectSelf };

  explicit ChildFrameDisconnector(Node& root,
                                  DisconnectReason disconnect_reason)
      : root_(&root), disconnect_reason_(disconnect_reason), previous_(last_) {
    last_ = this;
  }
  ~ChildFrameDisconnector() { last_ = previous_; }

  void Disconnect(DisconnectPolicy = kRootAndDescendants);

  // Returns whether `document`, which is being detached, should dispatch
  // pagehide/visibilitychange/unload. When
  // OmitSubframeDetachmentEventsOnRemoval is enabled, these are omitted if the
  // detachment is caused by a DOM node removal in a document that shares an
  // agent with `document`.
  static bool ShouldFireUnloadEvents(const Document&);

 private:
  void CollectFrameOwners(Node&);
  void DisconnectCollectedFrameOwners();
  Node& Root() const { return *root_; }

  HeapVector<Member<HTMLFrameOwnerElement>, 10> frame_owners_;
  Node* root_;
  DisconnectReason disconnect_reason_;
  ChildFrameDisconnector* previous_;
  static ChildFrameDisconnector* last_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CHILD_FRAME_DISCONNECTOR_H_
