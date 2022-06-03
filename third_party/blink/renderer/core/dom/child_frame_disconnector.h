// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CHILD_FRAME_DISCONNECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CHILD_FRAME_DISCONNECTOR_H_

#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class HTMLFrameOwnerElement;
class Node;

class ChildFrameDisconnector {
  STACK_ALLOCATED();

 public:
  enum DisconnectPolicy { kRootAndDescendants, kDescendantsOnly };

  explicit ChildFrameDisconnector(Node& root) : root_(&root) {}

  void Disconnect(DisconnectPolicy = kRootAndDescendants);

 private:
  void CollectFrameOwners(Node&);
  void DisconnectCollectedFrameOwners();
  Node& Root() const { return *root_; }

  HeapVector<Member<HTMLFrameOwnerElement>, 10> frame_owners_;
  Node* root_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CHILD_FRAME_DISCONNECTOR_H_
