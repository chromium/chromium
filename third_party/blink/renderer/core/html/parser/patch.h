// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_PATCH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_PATCH_H_

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {
class ContainerNode;
class HTMLTemplateElement;
class Node;
class HTMLTemplateElement;
struct HTMLConstructionSiteTask;

class Patch : public GarbageCollected<Patch> {
 public:
  static Patch* Prepare(ContainerNode* scope,
                        const AtomicString& target,
                        HTMLTemplateElement*);
  void Apply(HTMLConstructionSiteTask&);
  void Finalize(HTMLTemplateElement*);
  bool is_buffered() const { return is_buffered_; }
  ContainerNode* parent() const { return parent_; }
  void Trace(Visitor* visitor) const;

  Patch(base::PassKey<Patch>,
        ContainerNode* parent,
        Node* start_marker,
        Node* end_marker,
        bool is_buffered)
      : parent_(parent),
        start_marker_(start_marker),
        end_marker_(end_marker),
        is_buffered_(is_buffered) {}

 private:
  Member<ContainerNode> parent_;
  Member<Node> start_marker_;
  Member<Node> end_marker_;
  bool is_buffered_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_PATCH_H_
