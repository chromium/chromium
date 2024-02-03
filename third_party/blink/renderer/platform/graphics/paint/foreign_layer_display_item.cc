// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"

#include <utility>

#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "third_party/blink/renderer/platform/graphics/compositing/layers_as_json.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"

namespace blink {

ForeignLayerDisplayItem::ForeignLayerDisplayItem(
    DisplayItemClientId client_id,
    Type type,
    scoped_refptr<cc::Layer> layer,
    const gfx::Point& origin,
    RasterEffectOutset outset,
    PaintInvalidationReason paint_invalidation_reason)
    : DisplayItem(client_id,
                  type,
                  gfx::Rect(origin, layer->bounds()),
                  outset,
                  paint_invalidation_reason,
                  /*draws_content*/ true),
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
                        const gfx::Point& origin,
                        const PropertyTreeStateOrAlias* properties) {
  PaintController& paint_controller = context.GetPaintController();
  // This is like ScopedPaintChunkProperties but uses null id because foreign
  // layer chunk doesn't need an id nor a client.
  std::optional<PropertyTreeStateOrAlias> previous_properties;
  if (properties) {
    previous_properties.emplace(paint_controller.CurrentPaintChunkProperties());
    paint_controller.UpdateCurrentPaintChunkProperties(*properties);
  }
  paint_controller.CreateAndAppend<ForeignLayerDisplayItem>(
      client, type, std::move(layer), origin,
      client.VisualRectOutsetForRasterEffects(),
      client.GetPaintInvalidationReason());
  if (properties) {
    paint_controller.UpdateCurrentPaintChunkProperties(*previous_properties);
  }
}

}  // namespace blink
