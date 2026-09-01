// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_PATCH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_PATCH_H_

#include "third_party/blink/renderer/core/html/parser/html_construction_site.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {
class ContainerNode;
class ExternalPatchLoader;
class HTMLTemplateElement;
class Node;

// This class manages the lifecycle of out-of-order streaming (<template for>
// during parsing).
class Patch : public GarbageCollected<Patch> {
 public:
  static Patch* Prepare(ContainerNode* scope,
                        const AtomicString& target,
                        HTMLTemplateElement*);

  virtual ~Patch() = default;

  void Apply(HTMLConstructionSite::InsertionLocation&);
  void Finalize(HTMLTemplateElement*);
  virtual void DidFinishParsingChildren(HTMLTemplateElement*);
  void DidRemoveTemplateElement();

  ContainerNode* parent() const { return parent_.Get(); }
  Node* end_marker() const { return end_marker_.Get(); }
  bool is_buffered() const { return is_buffered_; }
  bool IsExternal() const { return !!loader_; }

  virtual void Trace(Visitor* visitor) const;

  Patch(base::PassKey<Patch>,
        ContainerNode* parent,
        Node* start_marker,
        Node* end_marker,
        bool is_buffered,
        HTMLTemplateElement* template_element);

 private:
  Member<ContainerNode> parent_;
  Member<Node> start_marker_;
  Member<Node> end_marker_;
  bool is_buffered_;

  Member<ExternalPatchLoader> loader_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_PATCH_H_
