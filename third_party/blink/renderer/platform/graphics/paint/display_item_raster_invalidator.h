// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_DISPLAY_ITEM_RASTER_INVALIDATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_DISPLAY_ITEM_RASTER_INVALIDATOR_H_

#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidator.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Generates raster invalidation for changed display items in a chunk.
class DisplayItemRasterInvalidator {
  STACK_ALLOCATED();

 public:
  DisplayItemRasterInvalidator(
      RasterInvalidator& invalidator,
      RasterInvalidator::RasterInvalidationFunction function,
      const PaintArtifact& old_paint_artifact,
      const PaintArtifact& new_paint_artifact,
      const PaintChunk& old_chunk,
      const PaintChunk& new_chunk,
      const ChunkToLayerMapper& mapper)
      : invalidator_(invalidator),
        raster_invalidation_function_(function),
        old_paint_artifact_(old_paint_artifact),
        new_paint_artifact_(new_paint_artifact),
        old_chunk_(old_chunk),
        new_chunk_(new_chunk),
        mapper_(mapper) {
    DCHECK(old_chunk_.Matches(new_chunk_));
  }

  void Generate();

 private:
  static const auto kClientIsOld = RasterInvalidator::kClientIsOld;
  static const auto kClientIsNew = RasterInvalidator::kClientIsNew;

  ALWAYS_INLINE void AddRasterInvalidation(const DisplayItemClient&,
                                           const IntRect&,
                                           PaintInvalidationReason,
                                           RasterInvalidator::ClientIsOldOrNew);
  ALWAYS_INLINE wtf_size_t
  MatchNewDisplayItemInOldChunk(const DisplayItem& new_item,
                                wtf_size_t& next_old_item_to_match);
  ALWAYS_INLINE void GenerateRasterInvalidation(const DisplayItemClient&,
                                                const IntRect& old_visual_rect,
                                                const IntRect& new_visual_rect,
                                                PaintInvalidationReason);
  ALWAYS_INLINE void GenerateIncrementalRasterInvalidation(
      const DisplayItemClient&,
      const IntRect& old_visual_rect,
      const IntRect& new_visual_rect);
  ALWAYS_INLINE void GenerateFullRasterInvalidation(
      const DisplayItemClient&,
      const IntRect& old_visual_rect,
      const IntRect& new_visual_rect,
      PaintInvalidationReason reason);

  RasterInvalidator& invalidator_;
  RasterInvalidator::RasterInvalidationFunction raster_invalidation_function_;
  const PaintArtifact& old_paint_artifact_;
  const PaintArtifact& new_paint_artifact_;
  const PaintChunk& old_chunk_;
  const PaintChunk& new_chunk_;
  const ChunkToLayerMapper& mapper_;
  // Maps clients to indices of display items in old_chunk_.
  HashMap<const DisplayItemClient*, Vector<wtf_size_t>>
      old_display_items_index_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DISPLAY_ITEM_PAINT_RASTER_INVALIDATOR_H_
