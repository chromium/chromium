/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
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

#include "third_party/blink/renderer/core/dom/shadow_root_v0.h"

#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/v0_insertion_point.h"
#include "third_party/blink/renderer/core/html/html_content_element.h"
#include "third_party/blink/renderer/core/html/html_shadow_element.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"

namespace blink {

class DistributionPool final {
  STACK_ALLOCATED();

 public:
  explicit DistributionPool(const ContainerNode&);
  void Clear();
  ~DistributionPool();
  void DistributeTo(V0InsertionPoint*, ShadowRoot*);
  void PopulateChildren(const ContainerNode&);

 private:
  void DetachNonDistributedNodes();
  HeapVector<Member<Node>, 32> nodes_;
  Vector<bool, 32> distributed_;
};

inline DistributionPool::DistributionPool(const ContainerNode& parent) {
  PopulateChildren(parent);
}

inline void DistributionPool::Clear() {
  DetachNonDistributedNodes();
  nodes_.clear();
  distributed_.clear();
}

inline void DistributionPool::PopulateChildren(const ContainerNode& parent) {
  Clear();
  for (Node* child = parent.firstChild(); child; child = child->nextSibling()) {
    // Re-distribution across v0 and v1 shadow trees is not supported
    if (IsA<HTMLSlotElement>(child))
      continue;

    if (IsActiveV0InsertionPoint(*child)) {
      auto* insertion_point = To<V0InsertionPoint>(child);
      for (wtf_size_t i = 0; i < insertion_point->DistributedNodesSize(); ++i)
        nodes_.push_back(insertion_point->DistributedNodeAt(i));
    } else {
      nodes_.push_back(child);
    }
  }
  distributed_.resize(nodes_.size());
  distributed_.Fill(false);
}

void DistributionPool::DistributeTo(V0InsertionPoint* insertion_point,
                                    ShadowRoot* shadow_root) {
  DistributedNodes distributed_nodes;

  for (wtf_size_t i = 0; i < nodes_.size(); ++i) {
    if (distributed_[i])
      continue;

    auto* html_content_element = DynamicTo<HTMLContentElement>(insertion_point);
    if (html_content_element && !html_content_element->CanSelectNode(nodes_, i))
      continue;

    Node* node = nodes_[i];
    distributed_nodes.Append(node);
    shadow_root->V0().DidDistributeNode(node, insertion_point);
    distributed_[i] = true;
  }

  // Distributes fallback elements
  if (insertion_point->IsContentInsertionPoint() &&
      distributed_nodes.IsEmpty()) {
    for (Node* fallback_node = insertion_point->firstChild(); fallback_node;
         fallback_node = fallback_node->nextSibling()) {
      distributed_nodes.Append(fallback_node);
      shadow_root->V0().DidDistributeNode(fallback_node, insertion_point);
    }
  }
  insertion_point->SetDistributedNodes(distributed_nodes);
}

inline DistributionPool::~DistributionPool() {
  DetachNonDistributedNodes();
}

inline void DistributionPool::DetachNonDistributedNodes() {
  for (wtf_size_t i = 0; i < nodes_.size(); ++i) {
    if (!distributed_[i])
      nodes_[i]->RemovedFromFlatTree();
  }
}

const HeapVector<Member<V0InsertionPoint>>&
ShadowRootV0::DescendantInsertionPoints() {
  DEFINE_STATIC_LOCAL(
      Persistent<HeapVector<Member<V0InsertionPoint>>>, empty_list,
      (MakeGarbageCollected<HeapVector<Member<V0InsertionPoint>>>()));
  if (descendant_insertion_points_is_valid_)
    return descendant_insertion_points_;

  descendant_insertion_points_is_valid_ = true;

  if (!ContainsInsertionPoints())
    return *empty_list;

  HeapVector<Member<V0InsertionPoint>> insertion_points;
  for (V0InsertionPoint& insertion_point :
       Traversal<V0InsertionPoint>::DescendantsOf(GetShadowRoot()))
    insertion_points.push_back(&insertion_point);

  descendant_insertion_points_.swap(insertion_points);
  return descendant_insertion_points_;
}

const V0InsertionPoint* ShadowRootV0::FinalDestinationInsertionPointFor(
    const Node* key) const {
#if DCHECK_IS_ON()
  DCHECK(key);
  // Allow traversal without V0 distribution up-to-date for marking ancestors
  // with ChildNeedsStyleRecalc() when FlatTreeStyleRecalc is enabled.
  DCHECK(!key->NeedsDistributionRecalc() ||
         RuntimeEnabledFeatures::FlatTreeStyleRecalcEnabled() &&
             key->GetDocument().AllowDirtyShadowV0Traversal());
#endif
  NodeToDestinationInsertionPoints::const_iterator it =
      node_to_insertion_points_.find(key);
  return it == node_to_insertion_points_.end() ? nullptr : it->value->back();
}

const DestinationInsertionPoints* ShadowRootV0::DestinationInsertionPointsFor(
    const Node* key) const {
  DCHECK(key);
  DCHECK(!key->NeedsDistributionRecalc());
  NodeToDestinationInsertionPoints::const_iterator it =
      node_to_insertion_points_.find(key);
  return it == node_to_insertion_points_.end() ? nullptr : it->value;
}

void ShadowRootV0::Distribute() {
  ClearDistribution();

  DistributionPool pool(GetShadowRoot().host());
  HTMLShadowElement* shadow_insertion_point = nullptr;

  for (const auto& point : DescendantInsertionPoints()) {
    if (!point->IsActive())
      continue;
    if (auto* shadow = DynamicTo<HTMLShadowElement>(*point)) {
      DCHECK(!shadow_insertion_point);
      shadow_insertion_point = shadow;
    } else {
      pool.DistributeTo(point, &GetShadowRoot());
      if (ShadowRoot* shadow_root =
              ShadowRootWhereNodeCanBeDistributedForV0(*point)) {
        if (!shadow_root->IsV1())
          shadow_root->SetNeedsDistributionRecalc();
      }
    }
  }

  if (shadow_insertion_point) {
    pool.DistributeTo(shadow_insertion_point, &GetShadowRoot());
    if (ShadowRoot* shadow_root =
            ShadowRootWhereNodeCanBeDistributedForV0(*shadow_insertion_point))
      shadow_root->SetNeedsDistributionRecalc();
  }
  probe::DidPerformElementShadowDistribution(&GetShadowRoot().host());
}

void ShadowRootV0::DidDistributeNode(const Node* node,
                                     V0InsertionPoint* insertion_point) {
  NodeToDestinationInsertionPoints::AddResult result =
      node_to_insertion_points_.insert(node, nullptr);
  if (result.is_new_entry) {
    result.stored_value->value =
        MakeGarbageCollected<DestinationInsertionPoints>();
  }
  result.stored_value->value->push_back(insertion_point);
}

void ShadowRootV0::ClearDistribution() {
  node_to_insertion_points_.clear();
}

void ShadowRootV0::WillAffectSelector() {
  for (ShadowRoot* shadow_root = &GetShadowRoot(); shadow_root;
       shadow_root = shadow_root->host().ContainingShadowRoot()) {
    if (shadow_root->IsV1() || shadow_root->V0().NeedsSelectFeatureSet())
      break;
    shadow_root->V0().SetNeedsSelectFeatureSet();
  }
  GetShadowRoot().SetNeedsDistributionRecalc();
}

const SelectRuleFeatureSet& ShadowRootV0::EnsureSelectFeatureSet() {
  if (!needs_select_feature_set_)
    return select_features_;

  select_features_.Clear();
  CollectSelectFeatureSetFrom();
  needs_select_feature_set_ = false;
  return SelectFeatures();
}

void ShadowRootV0::CollectSelectFeatureSetFrom() {
  if (!GetShadowRoot().ContainsShadowRoots() && !ContainsContentElements())
    return;

  auto& select_features = select_features_;
  for (Element& element : ElementTraversal::DescendantsOf(GetShadowRoot())) {
    if (ShadowRoot* shadow_root = element.GetShadowRoot()) {
      if (!shadow_root->IsV1())
        select_features.Add(shadow_root->V0().EnsureSelectFeatureSet());
    }
    if (auto* content = DynamicTo<HTMLContentElement>(element))
      select_features.CollectFeaturesFromSelectorList(content->SelectorList());
  }
}

}  // namespace blink
