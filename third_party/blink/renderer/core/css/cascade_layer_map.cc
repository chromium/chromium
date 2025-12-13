// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cascade_layer_map.h"

#include <compare>

#include "third_party/blink/renderer/core/css/cascade_layer.h"
#include "third_party/blink/renderer/core/css/rule_set.h"

namespace blink {
namespace {

// When building CascadeLayerMap (cascade_layer_map.h), we combine
// layers from all active RuleSets (layers with the same name are
// to be treated as the same layer; anonymous layers are all distinct),
// so that we can give them a canonical ordering (LayerOrderMap).
// This map contains one newly-created “merged layer” for each such
// group of equivalent layers.
using CanonicalLayerMap =
    HeapHashMap<Member<const CascadeLayer>, Member<const CascadeLayer>>;

using LayerOrderMap = HeapHashMap<Member<const CascadeLayer>, unsigned>;

void AddLayers(CascadeLayer* canonical_layer,
               const CascadeLayer& layer_from_sheet,
               CanonicalLayerMap& canonical_layer_map) {
  DCHECK_EQ(canonical_layer->GetName(), layer_from_sheet.GetName());
  canonical_layer_map.insert(&layer_from_sheet, canonical_layer);
  for (const auto& sub_layer_from_sheet :
       layer_from_sheet.GetDirectSubLayers()) {
    StyleRuleBase::LayerName sub_layer_name({sub_layer_from_sheet->GetName()});
    CascadeLayer* canonical_sub_layer =
        canonical_layer->GetOrAddSubLayer(sub_layer_name);
    AddLayers(canonical_sub_layer, *sub_layer_from_sheet, canonical_layer_map);
  }
}

void ComputeLayerOrder(CascadeLayer& layer,
                       uint16_t& next,
                       LayerOrderMap& canonical_layer_order_map) {
  for (const auto& sub_layer : layer.GetDirectSubLayers()) {
    ComputeLayerOrder(*sub_layer, next, canonical_layer_order_map);
  }
  canonical_layer_order_map.insert(&layer, next++);
}

}  // namespace

CascadeLayerMap::CascadeLayerMap(const ActiveStyleSheetVector& sheets) {
  CascadeLayer* canonical_root_layer = MakeGarbageCollected<CascadeLayer>();

  CanonicalLayerMap canonical_layer_map;
  for (const auto& sheet : sheets) {
    const RuleSet* rule_set = sheet.second;
    if (rule_set && rule_set->HasCascadeLayers()) {
      AddLayers(canonical_root_layer, rule_set->CascadeLayers(),
                canonical_layer_map);
    }
  }

  uint16_t next = 0;
  LayerOrderMap canonical_layer_order_map;
  ComputeLayerOrder(*canonical_root_layer, next, canonical_layer_order_map);
  canonical_layer_order_map.Set(canonical_root_layer, kImplicitOuterLayerOrder);

  canonical_root_layer_ = canonical_root_layer;

  for (const auto& iter : canonical_layer_map) {
    const CascadeLayer* layer_from_sheet = iter.key;
    const CascadeLayer* canonical_layer = iter.value;
    uint16_t layer_order = canonical_layer_order_map.at(canonical_layer);
    layer_order_map_.insert(layer_from_sheet, layer_order);

#if DCHECK_IS_ON()
    // The implicit outer layer is placed above all explicit layers.
    if (canonical_layer != canonical_root_layer_) {
      DCHECK_LT(layer_order, kImplicitOuterLayerOrder);
    }
#endif
  }
}

std::weak_ordering CascadeLayerMap::CompareLayerOrder(
    const CascadeLayer* lhs,
    const CascadeLayer* rhs) const {
  uint16_t lhs_order = lhs ? GetLayerOrder(*lhs) : kImplicitOuterLayerOrder;
  uint16_t rhs_order = rhs ? GetLayerOrder(*rhs) : kImplicitOuterLayerOrder;
  return lhs_order <=> rhs_order;
}

const CascadeLayer* CascadeLayerMap::GetRootLayer() const {
  return canonical_root_layer_.Get();
}

void CascadeLayerMap::Trace(blink::Visitor* visitor) const {
  visitor->Trace(layer_order_map_);
  visitor->Trace(canonical_root_layer_);
}

}  // namespace blink
