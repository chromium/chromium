// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_PATCH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_PATCH_H_

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {
class AtomicString;
class ContainerNode;
class Node;
struct HTMLConstructionSiteTask;

class Patch : public GarbageCollected<Patch> {
 public:
  static Patch* Prepare(ContainerNode* scope, const AtomicString& target);
  void Apply(HTMLConstructionSiteTask&);
  void Finalize();
  void Trace(Visitor* visitor) const;

  Patch(base::PassKey<Patch>,
        ContainerNode* parent,
        Node* start_marker,
        Node* end_marker)
      : parent_(parent), start_marker_(start_marker), end_marker_(end_marker) {}

 private:
  Member<ContainerNode> parent_;
  Member<Node> start_marker_;
  Member<Node> end_marker_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_PATCH_H_
