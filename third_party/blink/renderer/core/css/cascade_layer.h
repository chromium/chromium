// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CASCADE_LAYER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CASCADE_LAYER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

// A CascadeLayer object represents a node in the ordered tree of cascade layers
// in the sorted layer ordering.
// https://www.w3.org/TR/css-cascade-5/#layer-ordering
class CORE_EXPORT CascadeLayer final : public GarbageCollected<CascadeLayer> {
 public:
  explicit CascadeLayer(const AtomicString& name = g_empty_atom)
      : name_(name) {}
  ~CascadeLayer() = default;

  const AtomicString& GetName() const { return name_; }
  const HeapVector<Member<CascadeLayer>>& GetDirectSubLayers() const {
    return direct_sub_layers_;
  }

  // Getting or setting the order of a layer is only valid for canonical cascade
  // layers i.e. the unique layer representation for a particular tree scope.
  const absl::optional<unsigned> GetOrder() const { return order_; }
  void SetOrder(unsigned order) { order_ = order; }

  CascadeLayer* GetOrAddSubLayer(const StyleRuleBase::LayerName& name);

  void Trace(blink::Visitor*) const;

 private:
  friend class CascadeLayerTest;
  friend class RuleSetCascadeLayerTest;

  String ToStringForTesting() const;
  void ToStringInternal(StringBuilder&, const String&) const;

  CascadeLayer* FindDirectSubLayer(const AtomicString&) const;
  void ComputeLayerOrderInternal(unsigned* next);

  absl::optional<unsigned> order_;
  AtomicString name_;
  HeapVector<Member<CascadeLayer>> direct_sub_layers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CASCADE_LAYER_H_
