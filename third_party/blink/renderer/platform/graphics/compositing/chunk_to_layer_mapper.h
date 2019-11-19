// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_CHUNK_TO_LAYER_MAPPER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_CHUNK_TO_LAYER_MAPPER_H_

#include "third_party/blink/renderer/platform/graphics/paint/float_clip_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/skia/include/core/SkMatrix.h"

namespace blink {

struct PaintChunk;

// Maps geometry from PaintChunks to the containing composited layer.
// It provides higher performance than GeometryMapper by reusing computed
// transforms and clips for unchanged states within or across paint chunks.
class PLATFORM_EXPORT ChunkToLayerMapper {
  DISALLOW_NEW();

 public:
  ChunkToLayerMapper(
      const PropertyTreeState& layer_state,
      const gfx::Vector2dF& layer_offset,
      const FloatSize& visual_rect_subpixel_offset = FloatSize());

  // This class can map from multiple chunks. Before mapping from a chunk, this
  // method must be called to prepare for the chunk.
  void SwitchToChunk(const PaintChunk&);

  // Maps a visual rectangle in the current chunk space into the layer space.
  IntRect MapVisualRect(const IntRect&) const;

  // Returns the combined transform from the current chunk to the layer.
  SkMatrix Transform() const { return translation_2d_or_matrix_.ToSkMatrix(); }

  // Returns the combined clip from the current chunk to the layer if it can
  // be calculated (there is no filter that moves pixels), or infinite loose
  // clip rect otherwise.
  const FloatClipRect& ClipRect() const { return clip_rect_; }

 private:
  friend class ChunkToLayerMapperTest;

  IntRect MapUsingGeometryMapper(const IntRect&) const;
  void AdjustVisualRectBySubpixelOffset(FloatRect&) const;

  const PropertyTreeState layer_state_;
  const gfx::Vector2dF layer_offset_;
  const FloatSize visual_rect_subpixel_offset_;

  // The following fields are chunk-specific which are updated in
  // SwitchToChunk().
  PropertyTreeState chunk_state_;
  float outset_for_raster_effects_ = 0.f;
  GeometryMapper::Translation2DOrMatrix translation_2d_or_matrix_;
  FloatClipRect clip_rect_;
  // True if there is any pixel-moving filter between chunk state and layer
  // state, and we will call GeometryMapper for each mapping.
  bool has_filter_that_moves_pixels_ = false;
};

}  // namespace blink

#endif  // PaintArtifactCompositor_h
