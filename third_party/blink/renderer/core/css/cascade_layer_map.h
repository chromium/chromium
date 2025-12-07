// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CASCADE_LAYER_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CASCADE_LAYER_MAP_H_

#include <compare>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/active_style_sheets.h"
#include "third_party/blink/renderer/core/css/cascade_layered.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"

namespace blink {

class CascadeLayer;

// Gathers cascade layers from all style sheets in a tree scope, sorts them
// into the cascade layer ordering as per spec, and creates a mapping from
// layers in each sheet to the sorted layer order number.
class CORE_EXPORT CascadeLayerMap : public GarbageCollected<CascadeLayerMap> {
 public:
  static constexpr uint16_t kImplicitOuterLayerOrder =
      std::numeric_limits<uint16_t>::max();

  CascadeLayerMap(const ActiveStyleSheetVector& sheets);

  uint16_t GetLayerOrder(const CascadeLayer& layer) const {
    auto it = layer_order_map_.find(&layer);
    if (it != layer_order_map_.end()) {
      return it->value;
    }
    // We should not be doing lookup of layers that don't exist here,
    // but apparently that's possible (crbug.com/428664521).
    DCHECK(false);
    return kImplicitOuterLayerOrder;
  }

  // Compare the layer orders of two layered objects. The layers may originate
  // from different sheets, and one or both of them may be nullptr (indicating
  // the implicit root layer).
  template <typename T>
  static std::weak_ordering CompareLayerOrder(const CascadeLayerMap* layer_map,
                                              const CascadeLayered<T>& lhs,
                                              const CascadeLayered<T>& rhs) {
    // Note that layer_map may be nullptr here, but only when both layers
    // are nullptr.
    if (lhs.layer == rhs.layer) {
      return std::weak_ordering::equivalent;
    }
    CHECK(layer_map);
    return layer_map->CompareLayerOrder(lhs.layer, rhs.layer);
  }

  void Trace(blink::Visitor*) const;

  const CascadeLayer* GetRootLayer() const;

 private:
  std::weak_ordering CompareLayerOrder(const CascadeLayer* lhs,
                                       const CascadeLayer* rhs) const;

  Member<const CascadeLayer> canonical_root_layer_;
  HeapHashMap<Member<const CascadeLayer>, uint16_t> layer_order_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CASCADE_LAYER_MAP_H_
