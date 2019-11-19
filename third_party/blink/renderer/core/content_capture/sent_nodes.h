// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_SENT_NODES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_SENT_NODES_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Node;

// The class manages a list of nodes that have been sent, is only used when
// kNodeID is used, see WebContentCaptureClient::GetNodeType().
class SentNodes final : public GarbageCollected<SentNodes> {
 public:
  bool HasSent(const Node& node);
  void OnSent(const Node& node);

  void Trace(blink::Visitor*);

 private:
  HeapHashSet<WeakMember<const Node>> sent_nodes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_SENT_NODES_H_
