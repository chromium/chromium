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
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

namespace {

// It uses DebugName and OwnerNodeId of the input DisplayItemClient, while
// calculate VisualRect from the layer's offset and bounds.
class ForeignLayerDisplayItemClient final : public DisplayItemClient {
 public:
  ForeignLayerDisplayItemClient(const DisplayItemClient& client,
                                scoped_refptr<cc::Layer> layer,
                                const FloatPoint& offset)
      : client_(client), layer_(std::move(layer)), offset_(offset) {
    DCHECK(layer_);
    Invalidate(PaintInvalidationReason::kUncacheable);
  }

  String DebugName() const final { return client_.DebugName(); }

  DOMNodeId OwnerNodeId() const final { return client_.OwnerNodeId(); }

  IntRect VisualRect() const final {
    const auto& bounds = layer_->bounds();
    return EnclosingIntRect(
        FloatRect(offset_.X(), offset_.Y(), bounds.width(), bounds.height()));
  }

  cc::Layer* GetLayer() const { return layer_.get(); }

 private:
  const DisplayItemClient& client_;
  scoped_refptr<cc::Layer> layer_;
  FloatPoint offset_;
};

}  // anonymous namespace

ForeignLayerDisplayItem::ForeignLayerDisplayItem(
    const DisplayItemClient& client,
    Type type,
    scoped_refptr<cc::Layer> layer,
    const FloatPoint& offset,
    const LayerAsJSONClient* json_client)
    : DisplayItem(
          *new ForeignLayerDisplayItemClient(client, std::move(layer), offset),
          type,
          sizeof(*this)),
      offset_(offset),
      json_client_(json_client) {
  DCHECK(IsForeignLayerType(type));
  DCHECK(!IsCacheable());
}

ForeignLayerDisplayItem::~ForeignLayerDisplayItem() {
  delete &Client();
}

cc::Layer* ForeignLayerDisplayItem::GetLayer() const {
  return static_cast<const ForeignLayerDisplayItemClient&>(Client()).GetLayer();
}

const LayerAsJSONClient* ForeignLayerDisplayItem::GetLayerAsJSONClient() const {
  return json_client_;
}

bool ForeignLayerDisplayItem::Equals(const DisplayItem& other) const {
  return GetType() == other.GetType() &&
         GetLayer() ==
             static_cast<const ForeignLayerDisplayItem&>(other).GetLayer();
}

#if DCHECK_IS_ON()
void ForeignLayerDisplayItem::PropertiesAsJSON(JSONObject& json) const {
  DisplayItem::PropertiesAsJSON(json);
  json.SetInteger("layer", GetLayer()->id());
  json.SetDouble("offset_x", Offset().X());
  json.SetDouble("offset_y", Offset().Y());
}
#endif

static void RecordForeignLayerInternal(
    GraphicsContext& context,
    const DisplayItemClient& client,
    DisplayItem::Type type,
    scoped_refptr<cc::Layer> layer,
    const FloatPoint& offset,
    const LayerAsJSONClient* json_client,
    const base::Optional<PropertyTreeState>& properties) {
  PaintController& paint_controller = context.GetPaintController();
  if (paint_controller.DisplayItemConstructionIsDisabled())
    return;

  // This is like ScopedPaintChunkProperties but uses null id because foreign
  // layer chunk doesn't need an id nor a client.
  base::Optional<PropertyTreeState> previous_properties;
  if (properties) {
    previous_properties.emplace(paint_controller.CurrentPaintChunkProperties());
    paint_controller.UpdateCurrentPaintChunkProperties(base::nullopt,
                                                       *properties);
  }
  paint_controller.CreateAndAppend<ForeignLayerDisplayItem>(
      client, type, std::move(layer), offset, json_client);
  if (properties) {
    paint_controller.UpdateCurrentPaintChunkProperties(base::nullopt,
                                                       *previous_properties);
  }
}

void RecordForeignLayer(GraphicsContext& context,
                        const DisplayItemClient& client,
                        DisplayItem::Type type,
                        scoped_refptr<cc::Layer> layer,
                        const FloatPoint& offset,
                        const base::Optional<PropertyTreeState>& properties) {
  RecordForeignLayerInternal(context, client, type, std::move(layer), offset,
                             nullptr, properties);
}

void RecordGraphicsLayerAsForeignLayer(GraphicsContext& context,
                                       DisplayItem::Type type,
                                       const GraphicsLayer& graphics_layer) {
  // In pre-CompositeAfterPaint, the GraphicsLayer hierarchy is still built
  // during CompositingUpdate, and we have to clear them here to ensure no
  // extraneous layers are still attached. In future we will disable all
  // those layer hierarchy code so we won't need this line.
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  graphics_layer.CcLayer()->RemoveAllChildren();

  RecordForeignLayerInternal(
      context, graphics_layer, type, graphics_layer.CcLayer(),
      FloatPoint(graphics_layer.GetOffsetFromTransformNode()), &graphics_layer,
      graphics_layer.GetPropertyTreeState());
}

}  // namespace blink
