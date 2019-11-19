// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_SCOPED_PAINT_CHUNK_PROPERTIES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_SCOPED_PAINT_CHUNK_PROPERTIES_H_

#include "base/macros.h"
#include "base/optional.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ScopedPaintChunkProperties {
  DISALLOW_NEW();

 public:
  // Use new PropertyTreeState for the scope.
  ScopedPaintChunkProperties(PaintController& paint_controller,
                             const PropertyTreeState& properties,
                             const DisplayItemClient& client,
                             DisplayItem::Type type)
      : paint_controller_(paint_controller),
        previous_properties_(paint_controller.CurrentPaintChunkProperties()) {
    paint_controller_.UpdateCurrentPaintChunkProperties(
        PaintChunk::Id(client, type), properties);
  }

  // Use new transform state, and keep the current other properties.
  ScopedPaintChunkProperties(PaintController& paint_controller,
                             const TransformPaintPropertyNode& transform,
                             const DisplayItemClient& client,
                             DisplayItem::Type type)
      : ScopedPaintChunkProperties(
            paint_controller,
            GetPaintChunkProperties(transform, paint_controller),
            client,
            type) {}

  // Use new clip state, and keep the current other properties.
  ScopedPaintChunkProperties(PaintController& paint_controller,
                             const ClipPaintPropertyNode& clip,
                             const DisplayItemClient& client,
                             DisplayItem::Type type)
      : ScopedPaintChunkProperties(
            paint_controller,
            GetPaintChunkProperties(clip, paint_controller),
            client,
            type) {}

  // Use new effect state, and keep the current other properties.
  ScopedPaintChunkProperties(PaintController& paint_controller,
                             const EffectPaintPropertyNode& effect,
                             const DisplayItemClient& client,
                             DisplayItem::Type type)
      : ScopedPaintChunkProperties(
            paint_controller,
            GetPaintChunkProperties(effect, paint_controller),
            client,
            type) {}

  ~ScopedPaintChunkProperties() {
    // We should not return to the previous id, because that may cause a new
    // chunk to use the same id as that of the previous chunk before this
    // ScopedPaintChunkProperties. The painter should create another scope of
    // paint properties with new id, or the new chunk will use the id of the
    // first display item as its id.
    paint_controller_.UpdateCurrentPaintChunkProperties(base::nullopt,
                                                        previous_properties_);
  }

 private:
  static PropertyTreeState GetPaintChunkProperties(
      const TransformPaintPropertyNode& transform,
      PaintController& paint_controller) {
    PropertyTreeState properties(
        paint_controller.CurrentPaintChunkProperties());
    properties.SetTransform(transform);
    return properties;
  }

  static PropertyTreeState GetPaintChunkProperties(
      const ClipPaintPropertyNode& clip,
      PaintController& paint_controller) {
    PropertyTreeState properties(
        paint_controller.CurrentPaintChunkProperties());
    properties.SetClip(clip);
    return properties;
  }

  static PropertyTreeState GetPaintChunkProperties(
      const EffectPaintPropertyNode& effect,
      PaintController& paint_controller) {
    PropertyTreeState properties(
        paint_controller.CurrentPaintChunkProperties());
    properties.SetEffect(effect);
    return properties;
  }

  PaintController& paint_controller_;
  PropertyTreeState previous_properties_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPaintChunkProperties);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_SCOPED_PAINT_CHUNK_PROPERTIES_H_
