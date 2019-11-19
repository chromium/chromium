/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_V0_INSERTION_POINT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_V0_INSERTION_POINT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/dom/distributed_nodes.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CORE_EXPORT V0InsertionPoint : public HTMLElement {
 public:
  ~V0InsertionPoint() override;

  bool HasDistribution() const { return !distributed_nodes_.IsEmpty(); }
  void SetDistributedNodes(DistributedNodes&);
  void ClearDistribution() { distributed_nodes_.Clear(); }
  bool IsActive() const;
  bool CanBeActive() const;

  bool IsContentInsertionPoint() const;

  StaticNodeList* getDistributedNodes();

  virtual bool CanAffectSelector() const { return false; }

  void AttachLayoutTree(AttachContext&) override;
  void DetachLayoutTree(bool performing_reattach) override;
  void RebuildDistributedChildrenLayoutTrees(WhitespaceAttacher&);

  size_t DistributedNodesSize() const { return distributed_nodes_.size(); }
  Node* DistributedNodeAt(wtf_size_t index) const {
    return distributed_nodes_.at(index);
  }
  Node* FirstDistributedNode() const {
    return distributed_nodes_.IsEmpty() ? nullptr : distributed_nodes_.First();
  }
  Node* LastDistributedNode() const {
    return distributed_nodes_.IsEmpty() ? nullptr : distributed_nodes_.Last();
  }
  Node* DistributedNodeNextTo(const Node* node) const {
    return distributed_nodes_.NextTo(node);
  }
  Node* DistributedNodePreviousTo(const Node* node) const {
    return distributed_nodes_.PreviousTo(node);
  }
  bool DistributedNodesAreFallback() const {
    // We either do not have distributed children or the distributed children
    // are the fallback children.
    return !HasDistribution() || DistributedNodeAt(0)->parentNode() == this;
  }

  void RecalcStyleForInsertionPointChildren(const StyleRecalcChange);

  void Trace(Visitor*) override;

 protected:
  V0InsertionPoint(const QualifiedName&, Document&);
  bool LayoutObjectIsNeeded(const ComputedStyle&) const override;
  void ChildrenChanged(const ChildrenChange&) override;
  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;
  void DidRecalcStyle(const StyleRecalcChange) override;

 private:
  bool IsV0InsertionPoint() const =
      delete;  // This will catch anyone doing an unnecessary check.

  DistributedNodes distributed_nodes_;
  bool registered_with_shadow_root_;
};

using DestinationInsertionPoints = HeapVector<Member<V0InsertionPoint>, 1>;

inline bool IsActiveV0InsertionPoint(const Node& node) {
  auto* insertion_point = DynamicTo<V0InsertionPoint>(node);
  return insertion_point && insertion_point->IsActive();
}

inline ShadowRoot* ShadowRootWhereNodeCanBeDistributedForV0(const Node& node) {
  Node* parent = node.parentNode();
  if (!parent)
    return nullptr;
  if (IsActiveV0InsertionPoint(*parent))
    return node.ContainingShadowRoot();
  if (auto* parent_element = DynamicTo<Element>(parent))
    return parent_element->GetShadowRoot();
  return nullptr;
}

const V0InsertionPoint* ResolveReprojection(const Node*);

void CollectDestinationInsertionPoints(
    const Node&,
    HeapVector<Member<V0InsertionPoint>, 8>& results);

template <>
inline bool IsElementOfType<const V0InsertionPoint>(const Node& node) {
  return node.IsV0InsertionPoint();
}

template <>
struct DowncastTraits<V0InsertionPoint> {
  static bool AllowFrom(const Node& node) { return node.IsV0InsertionPoint(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_V0_INSERTION_POINT_H_
