// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"

#include <utility>

#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "third_party/blink/renderer/platform/graphics/compositing/layers_as_json.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"

namespace blink {

ForeignLayerDisplayItem::ForeignLayerDisplayItem(
    const DisplayItemClient& client,
    Type type,
    scoped_refptr<cc::Layer> layer,
    const IntPoint& offset,
    PaintInvalidationReason paint_invalidation_reason)
    : DisplayItem(client,
                  type,
                  IntRect(offset, IntSize(layer->bounds())),
                  paint_invalidation_reason),
      layer_(std::move(layer)) {
  DCHECK(IsForeignLayerType(type));
}

bool ForeignLayerDisplayItem::EqualsForUnderInvalidationImpl(
    const ForeignLayerDisplayItem& other) const {
  DCHECK(RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled());
  return GetLayer() == other.GetLayer();
}

#if DCHECK_IS_ON()
void ForeignLayerDisplayItem::PropertiesAsJSONImpl(JSONObject& json) const {
  json.SetInteger("layer", GetLayer()->id());
}
#endif

void RecordForeignLayer(GraphicsContext& context,
                        const DisplayItemClient& client,
                        DisplayItem::Type type,
                        scoped_refptr<cc::Layer> layer,
                        const IntPoint& offset,
                        const PropertyTreeStateOrAlias* properties) {
  PaintController& paint_controller = context.GetPaintController();
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    // Only record the first fragment's cc::Layer to prevent duplicate layers.
    // This is not needed for link highlights which do support fragmentation.
    if (type != DisplayItem::kForeignLayerLinkHighlight &&
        paint_controller.CurrentFragment() != 0) {
      return;
    }
  }
  // This is like ScopedPaintChunkProperties but uses null id because foreign
  // layer chunk doesn't need an id nor a client.
  absl::optional<PropertyTreeStateOrAlias> previous_properties;
  if (properties) {
    previous_properties.emplace(paint_controller.CurrentPaintChunkProperties());
    paint_controller.UpdateCurrentPaintChunkProperties(nullptr, *properties);
  }
  paint_controller.CreateAndAppend<ForeignLayerDisplayItem>(
      client, type, std::move(layer), offset,
      client.GetPaintInvalidationReason());
  if (properties) {
    paint_controller.UpdateCurrentPaintChunkProperties(nullptr,
                                                       *previous_properties);
  }
}

}  // namespace blink
