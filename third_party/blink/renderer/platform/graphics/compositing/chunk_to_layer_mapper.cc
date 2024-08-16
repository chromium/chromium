// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/chunk_to_layer_mapper.h"

#include "base/logging.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

ChunkToLayerMapper::ChunkToLayerMapper(const PropertyTreeState& layer_state,
                                       const gfx::Vector2dF& layer_offset)
    : layer_state_(layer_state),
      layer_offset_(layer_offset),
      chunk_state_(layer_state_),
      transform_(gfx::Transform::MakeTranslation(-layer_offset)) {}

void ChunkToLayerMapper::SwitchToChunk(const PaintChunk& chunk) {
  SwitchToChunkWithState(chunk, chunk.properties.Unalias());
}

void ChunkToLayerMapper::SwitchToChunkWithState(
    const PaintChunk& chunk,
    const PropertyTreeState& new_chunk_state) {
  raster_effect_outset_ = chunk.raster_effect_outset;

  DCHECK_EQ(new_chunk_state, chunk.properties.Unalias());
  if (new_chunk_state == chunk_state_) {
    return;
  }

  if (new_chunk_state == layer_state_) {
    has_filter_that_moves_pixels_ = false;
    transform_ = gfx::Transform::MakeTranslation(-layer_offset_);
    clip_rect_ = FloatClipRect();
    chunk_state_ = new_chunk_state;
    return;
  }

  if (&new_chunk_state.Transform() != &chunk_state_.Transform()) {
    transform_ = GeometryMapper::SourceToDestinationProjection(
        new_chunk_state.Transform(), layer_state_.Transform());
    transform_.PostTranslate(-layer_offset_);
  }

  has_filter_that_moves_pixels_ =
      new_chunk_state.Clip().NearestPixelMovingFilterClip() !=
      layer_state_.Clip().NearestPixelMovingFilterClip();

  if (has_filter_that_moves_pixels_) {
    clip_rect_ = InfiniteLooseFloatClipRect();
  } else if (&new_chunk_state.Clip() != &chunk_state_.Clip()) {
    clip_rect_ =
        GeometryMapper::LocalToAncestorClipRect(new_chunk_state, layer_state_);
    if (!clip_rect_.IsInfinite())
      clip_rect_.Move(-layer_offset_);
  }

  chunk_state_ = new_chunk_state;
}

gfx::Rect ChunkToLayerMapper::MapVisualRect(const gfx::Rect& rect) const {
  if (rect.IsEmpty())
    return gfx::Rect();

  if (has_filter_that_moves_pixels_) [[unlikely]] {
    return MapUsingGeometryMapper(rect);
  }

  gfx::RectF mapped_rect = transform_.MapRect(gfx::RectF(rect));
  if (!mapped_rect.IsEmpty() && !clip_rect_.IsInfinite())
    mapped_rect.Intersect(clip_rect_.Rect());

  gfx::Rect result;
  if (!mapped_rect.IsEmpty()) {
    InflateForRasterEffectOutset(mapped_rect);
    result = gfx::ToEnclosingRect(mapped_rect);
  }
#if DCHECK_IS_ON()
  auto slow_result = MapUsingGeometryMapper(rect);
  if (result != slow_result) {
    // Not a DCHECK because this may result from a floating point error.
    LOG(WARNING) << "ChunkToLayerMapper::MapVisualRect: Different results from"
                 << "fast path (" << result.ToString() << ") and slow path ("
                 << slow_result.ToString() << ")";
  }
#endif
  return result;
}

// This is called when the fast path doesn't apply if there is any filter that
// moves pixels. GeometryMapper::LocalToAncestorVisualRect() will apply the
// visual effects of the filters, though slowly.
gfx::Rect ChunkToLayerMapper::MapUsingGeometryMapper(
    const gfx::Rect& rect) const {
  return MapVisualRectFromState(rect, chunk_state_);
}

gfx::Rect ChunkToLayerMapper::MapVisualRectFromState(
    const gfx::Rect& rect,
    const PropertyTreeState& state) const {
  FloatClipRect visual_rect((gfx::RectF(rect)));
  GeometryMapper::LocalToAncestorVisualRect(state, layer_state_, visual_rect);
  if (visual_rect.Rect().IsEmpty()) {
    return gfx::Rect();
  }

  gfx::RectF result = visual_rect.Rect();
  result.Offset(-layer_offset_);
  InflateForRasterEffectOutset(result);
  return gfx::ToEnclosingRect(result);
}

void ChunkToLayerMapper::InflateForRasterEffectOutset(gfx::RectF& rect) const {
  if (raster_effect_outset_ == RasterEffectOutset::kHalfPixel)
    rect.Outset(0.5);
  else if (raster_effect_outset_ == RasterEffectOutset::kWholePixel)
    rect.Outset(1);
}

}  // namespace blink
