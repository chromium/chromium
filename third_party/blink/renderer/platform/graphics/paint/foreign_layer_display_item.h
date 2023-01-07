// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_FOREIGN_LAYER_DISPLAY_ITEM_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_FOREIGN_LAYER_DISPLAY_ITEM_H_

#include "base/dcheck_is_on.h"
#include "cc/layers/layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_client.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class DisplayItemClient;
class GraphicsContext;

// Represents foreign content (produced outside Blink) which draws to a layer.
// A client supplies a layer which can be unwrapped and inserted into the full
// layer tree.
//
// Before CAP, this content is not painted, but is instead inserted into the
// GraphicsLayer tree.
class PLATFORM_EXPORT ForeignLayerDisplayItem : public DisplayItem {
 public:
  ForeignLayerDisplayItem(DisplayItemClientId client_id,
                          Type,
                          scoped_refptr<cc::Layer>,
                          const gfx::Point& origin,
                          RasterEffectOutset,
                          PaintInvalidationReason);

  cc::Layer* GetLayer() const {
    DCHECK(!IsTombstone());
    return layer_.get();
  }

 private:
  friend class DisplayItem;
  bool EqualsForUnderInvalidationImpl(const ForeignLayerDisplayItem&) const;
#if DCHECK_IS_ON()
  void PropertiesAsJSONImpl(JSONObject&) const;
#endif

  scoped_refptr<cc::Layer> layer_;
};

// When a foreign layer's debug name is a literal string, define a instance of
// LiteralDebugNameClient with DEFINE_STATIC_LOCAL() and pass the instance as
// client to RecordForeignLayer().
class LiteralDebugNameClient : public GarbageCollected<LiteralDebugNameClient>,
                               public DisplayItemClient {
 public:
  LiteralDebugNameClient(const char* name) : name_(name) {}

  String DebugName() const override { return name_; }
  void Trace(Visitor* visitor) const override {
    DisplayItemClient::Trace(visitor);
  }

 private:
  const char* name_;
};

template <>
struct DowncastTraits<ForeignLayerDisplayItem> {
  static bool AllowFrom(const DisplayItem& i) {
    return !i.IsTombstone() && i.IsForeignLayer();
  }
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
    const gfx::Point& origin,
    const PropertyTreeStateOrAlias* properties = nullptr);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_FOREIGN_LAYER_DISPLAY_ITEM_H_
