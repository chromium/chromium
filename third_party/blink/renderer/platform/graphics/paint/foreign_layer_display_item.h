// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_FOREIGN_LAYER_DISPLAY_ITEM_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_FOREIGN_LAYER_DISPLAY_ITEM_H_

#include "cc/layers/layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class GraphicsContext;
class GraphicsLayer;
class LayerAsJSONClient;

// Represents foreign content (produced outside Blink) which draws to a layer.
// A client supplies a layer which can be unwrapped and inserted into the full
// layer tree.
//
// Before CAP, this content is not painted, but is instead inserted into the
// GraphicsLayer tree.
class PLATFORM_EXPORT ForeignLayerDisplayItem final : public DisplayItem {
 public:
  ForeignLayerDisplayItem(const DisplayItemClient& client,
                          Type,
                          scoped_refptr<cc::Layer>,
                          const FloatPoint& offset,
                          const LayerAsJSONClient*);
  ~ForeignLayerDisplayItem() override;

  cc::Layer* GetLayer() const;

  const LayerAsJSONClient* GetLayerAsJSONClient() const;

  // DisplayItem
  bool Equals(const DisplayItem&) const override;
#if DCHECK_IS_ON()
  void PropertiesAsJSON(JSONObject&) const override;
#endif

  FloatPoint Offset() const { return offset_; }

 private:
  FloatPoint offset_;
  const LayerAsJSONClient* json_client_;
};

// When a foreign layer's debug name is a literal string, define a instance of
// LiteralDebugNameClient with DEFINE_STATIC_LOCAL() and pass the instance as
// client to RecordForeignLayer().
class LiteralDebugNameClient : public DisplayItemClient {
 public:
  LiteralDebugNameClient(const char* name) : name_(name) {}

  String DebugName() const override { return name_; }
  IntRect VisualRect() const override {
    NOTREACHED();
    return IntRect();
  }

 private:
  const char* name_;
};

// Records a foreign layer into a GraphicsContext.
// Use this where you would use a recorder class.
// |client| provides DebugName and optionally DOMNodeId, while VisualRect will
// be calculated automatically based on layer bounds and offset.
PLATFORM_EXPORT void RecordForeignLayer(
    GraphicsContext& context,
    const DisplayItemClient& client,
    DisplayItem::Type type,
    scoped_refptr<cc::Layer> layer,
    const FloatPoint& offset,
    const base::Optional<PropertyTreeState>& = base::nullopt);

// Records a graphics layer into a GraphicsContext.
PLATFORM_EXPORT void RecordGraphicsLayerAsForeignLayer(
    GraphicsContext& context,
    DisplayItem::Type type,
    const GraphicsLayer& graphics_layer);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_FOREIGN_LAYER_DISPLAY_ITEM_H_
