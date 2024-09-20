// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cascade_layer_map.h"

#include "third_party/blink/renderer/core/css/rule_set.h"

namespace blink {
namespace {

// See layer_map.h.
using CanonicalLayerMap = LayerMap;

void ComputeLayerOrder(CascadeLayer& layer, uint16_t& next) {
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
      canonical_root_layer->Merge(rule_set->CascadeLayers(),
                                  canonical_layer_map);
    }
  }

  uint16_t next = 0;
  ComputeLayerOrder(*canonical_root_layer, next);

  canonical_root_layer->SetOrder(kImplicitOuterLayerOrder);
  canonical_root_layer_ = canonical_root_layer;

  for (const auto& iter : canonical_layer_map) {
    const CascadeLayer* layer_from_sheet = iter.key;
    const CascadeLayer* canonical_layer = iter.value;
    uint16_t layer_order = canonical_layer->GetOrder().value();
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
  uint16_t lhs_order = lhs ? GetLayerOrder(*lhs) : kImplicitOuterLayerOrder;
  uint16_t rhs_order = rhs ? GetLayerOrder(*rhs) : kImplicitOuterLayerOrder;
  return lhs_order < rhs_order ? -1 : (lhs_order > rhs_order ? 1 : 0);
}

const CascadeLayer* CascadeLayerMap::GetRootLayer() const {
  return canonical_root_layer_.Get();
}

void CascadeLayerMap::Trace(blink::Visitor* visitor) const {
  visitor->Trace(layer_order_map_);
  visitor->Trace(canonical_root_layer_);
}

}  // namespace blink
