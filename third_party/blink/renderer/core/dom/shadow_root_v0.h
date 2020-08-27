/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SHADOW_ROOT_V0_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SHADOW_ROOT_V0_H_

#include "third_party/blink/renderer/core/css/select_rule_feature_set.h"
#include "third_party/blink/renderer/core/dom/v0_insertion_point.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CORE_EXPORT ShadowRootV0 final : public GarbageCollected<ShadowRootV0> {
 public:
  using NodeToDestinationInsertionPoints =
      HeapHashMap<Member<const Node>, Member<DestinationInsertionPoints>>;

  explicit ShadowRootV0(ShadowRoot& shadow_root) : shadow_root_(&shadow_root) {}
  ShadowRootV0(const ShadowRootV0&) = delete;
  ShadowRootV0& operator=(const ShadowRootV0&) = delete;

  bool ContainsShadowElements() const {
    return descendant_shadow_element_count_;
  }
  bool ContainsContentElements() const {
    return descendant_content_element_count_;
  }
  bool ContainsInsertionPoints() const {
    return ContainsShadowElements() || ContainsContentElements();
  }

  unsigned DescendantShadowElementCount() const {
    return descendant_shadow_element_count_;
  }
  void DidAddInsertionPoint(V0InsertionPoint*);
  void DidRemoveInsertionPoint(V0InsertionPoint*);

  const HeapVector<Member<V0InsertionPoint>>& DescendantInsertionPoints();
  void InvalidateDescendantInsertionPoints();

  const V0InsertionPoint* FinalDestinationInsertionPointFor(
      const Node* key) const;
  const DestinationInsertionPoints* DestinationInsertionPointsFor(
      const Node* key) const;

  void Distribute();
  void DidDistributeNode(const Node*, V0InsertionPoint*);
  void ClearDistribution();

  void WillAffectSelector();
  const SelectRuleFeatureSet& EnsureSelectFeatureSet();
  void CollectSelectFeatureSetFrom();
  bool NeedsSelectFeatureSet() const { return needs_select_feature_set_; }
  void SetNeedsSelectFeatureSet() { needs_select_feature_set_ = true; }
  SelectRuleFeatureSet& SelectFeatures() { return select_features_; }

  void Trace(Visitor* visitor) const {
    visitor->Trace(shadow_root_);
    visitor->Trace(descendant_insertion_points_);
    visitor->Trace(node_to_insertion_points_);
  }

 private:
  ShadowRoot& GetShadowRoot() const { return *shadow_root_; }

  Member<ShadowRoot> shadow_root_;
  unsigned descendant_shadow_element_count_ = 0;
  unsigned descendant_content_element_count_ = 0;
  HeapVector<Member<V0InsertionPoint>> descendant_insertion_points_;

  NodeToDestinationInsertionPoints node_to_insertion_points_;
  SelectRuleFeatureSet select_features_;
  bool needs_select_feature_set_ = false;
  bool descendant_insertion_points_is_valid_ = false;
};

inline void ShadowRootV0::DidAddInsertionPoint(V0InsertionPoint* point) {
  DCHECK(point);
  if (IsA<HTMLShadowElement>(*point))
    ++descendant_shadow_element_count_;
  else if (IsA<HTMLContentElement>(*point))
    ++descendant_content_element_count_;
  else
    NOTREACHED();
  InvalidateDescendantInsertionPoints();
}

inline void ShadowRootV0::DidRemoveInsertionPoint(V0InsertionPoint* point) {
  DCHECK(point);
  if (IsA<HTMLShadowElement>(*point)) {
    DCHECK_GT(descendant_shadow_element_count_, 0u);
    --descendant_shadow_element_count_;
  } else if (IsA<HTMLContentElement>(*point)) {
    DCHECK_GT(descendant_content_element_count_, 0u);
    --descendant_content_element_count_;
  } else {
    NOTREACHED();
  }
  InvalidateDescendantInsertionPoints();
}

inline void ShadowRootV0::InvalidateDescendantInsertionPoints() {
  descendant_insertion_points_is_valid_ = false;
  descendant_insertion_points_.clear();
}

}  // namespace blink

#endif
