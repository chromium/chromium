// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cascade_layer_map.h"

#include "third_party/blink/renderer/core/css/rule_set.h"

namespace blink {
namespace {

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

void ComputeLayerOrder(CascadeLayer& layer, unsigned& next) {
  for (const auto& sub_layer : layer.GetDirectSubLayers()) {
    ComputeLayerOrder(*sub_layer, next);
  }
  layer.SetOrder(next++);
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

  unsigned next = 0;
  ComputeLayerOrder(*canonical_root_layer, next);

  canonical_root_layer->SetOrder(kImplicitOuterLayerOrder);
  canonical_root_layer_ = canonical_root_layer;

  for (const auto& iter : canonical_layer_map) {
    const CascadeLayer* layer_from_sheet = iter.key;
    const CascadeLayer* canonical_layer = iter.value;
    unsigned layer_order = canonical_layer->GetOrder().value();
    layer_order_map_.insert(layer_from_sheet, layer_order);

#if DCHECK_IS_ON()
    // The implicit outer layer is placed above all explicit layers.
    if (canonical_layer != canonical_root_layer_) {
      DCHECK_LT(layer_order, kImplicitOuterLayerOrder);
    }
#endif
  }
}

int CascadeLayerMap::CompareLayerOrder(const CascadeLayer* lhs,
                                       const CascadeLayer* rhs) const {
  unsigned lhs_order = lhs ? GetLayerOrder(*lhs) : kImplicitOuterLayerOrder;
  unsigned rhs_order = rhs ? GetLayerOrder(*rhs) : kImplicitOuterLayerOrder;
  return lhs_order < rhs_order ? -1 : (lhs_order > rhs_order ? 1 : 0);
}

const CascadeLayer* CascadeLayerMap::GetRootLayer() const {
  return canonical_root_layer_;
}

void CascadeLayerMap::Trace(blink::Visitor* visitor) const {
  visitor->Trace(layer_order_map_);
  visitor->Trace(canonical_root_layer_);
}

}  // namespace blink
