// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/chunk_to_layer_mapper.h"

#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"

namespace blink {

ChunkToLayerMapper::ChunkToLayerMapper(
    const PropertyTreeState& layer_state,
    const gfx::Vector2dF& layer_offset,
    const FloatSize& visual_rect_subpixel_offset)
    : layer_state_(layer_state.Unalias()),
      layer_offset_(layer_offset),
      visual_rect_subpixel_offset_(visual_rect_subpixel_offset),
      chunk_state_(layer_state_),
      translation_2d_or_matrix_(
          FloatSize(-layer_offset.x(), -layer_offset.y())) {}

void ChunkToLayerMapper::SwitchToChunk(const PaintChunk& chunk) {
  outset_for_raster_effects_ = chunk.outset_for_raster_effects;

  const auto& new_chunk_state =
      chunk.properties.GetPropertyTreeState().Unalias();
  if (new_chunk_state == chunk_state_)
    return;

  if (new_chunk_state == layer_state_) {
    has_filter_that_moves_pixels_ = false;
    translation_2d_or_matrix_ = GeometryMapper::Translation2DOrMatrix(
        FloatSize(-layer_offset_.x(), -layer_offset_.y()));
    clip_rect_ = FloatClipRect();
    chunk_state_ = new_chunk_state;
    return;
  }

  if (&new_chunk_state.Transform() != &chunk_state_.Transform()) {
    translation_2d_or_matrix_ = GeometryMapper::SourceToDestinationProjection(
        new_chunk_state.Transform(), layer_state_.Transform());
    translation_2d_or_matrix_.PostTranslate(-layer_offset_.x(),
                                            -layer_offset_.y());
  }

  bool new_has_filter_that_moves_pixels = has_filter_that_moves_pixels_;
  if (&new_chunk_state.Effect() != &chunk_state_.Effect()) {
    new_has_filter_that_moves_pixels = false;
    for (const auto* effect = &new_chunk_state.Effect();
         effect && effect != &layer_state_.Effect();
         effect = SafeUnalias(effect->Parent())) {
      if (effect->HasFilterThatMovesPixels()) {
        new_has_filter_that_moves_pixels = true;
        break;
      }
    }
  }

  bool needs_clip_recalculation =
      new_has_filter_that_moves_pixels != has_filter_that_moves_pixels_ ||
      &new_chunk_state.Clip() != &chunk_state_.Clip();
  if (needs_clip_recalculation) {
    clip_rect_ =
        GeometryMapper::LocalToAncestorClipRect(new_chunk_state, layer_state_);
    if (!clip_rect_.IsInfinite())
      clip_rect_.MoveBy(FloatPoint(-layer_offset_.x(), -layer_offset_.y()));
  }

  chunk_state_ = new_chunk_state;
  has_filter_that_moves_pixels_ = new_has_filter_that_moves_pixels;
}

IntRect ChunkToLayerMapper::MapVisualRect(const IntRect& rect) const {
  if (rect.IsEmpty())
    return IntRect();

  if (UNLIKELY(has_filter_that_moves_pixels_))
    return MapUsingGeometryMapper(rect);

  FloatRect mapped_rect(rect);
  translation_2d_or_matrix_.MapRect(mapped_rect);
  if (!mapped_rect.IsEmpty() && !clip_rect_.IsInfinite())
    mapped_rect.Intersect(clip_rect_.Rect());

  IntRect result;
  if (!mapped_rect.IsEmpty()) {
    mapped_rect.Inflate(outset_for_raster_effects_);
    AdjustVisualRectBySubpixelOffset(mapped_rect);
    result = EnclosingIntRect(mapped_rect);
  }
#if DCHECK_IS_ON()
  auto slow_result = MapUsingGeometryMapper(rect);
  if (result != slow_result) {
    // Not a DCHECK because this may result from a floating point error.
    LOG(WARNING) << "ChunkToLayerMapper::MapVisualRect: Different results from"
                 << "fast path (" << result << ") and slow path ("
                 << slow_result << ")";
  }
#endif
  return result;
}

// This is called when the fast path doesn't apply if there is any filter that
// moves pixels. GeometryMapper::LocalToAncestorVisualRect() will apply the
// visual effects of the filters, though slowly.
IntRect ChunkToLayerMapper::MapUsingGeometryMapper(const IntRect& rect) const {
  FloatClipRect visual_rect((FloatRect(rect)));
  GeometryMapper::LocalToAncestorVisualRect(chunk_state_, layer_state_,
                                            visual_rect);
  if (visual_rect.Rect().IsEmpty())
    return IntRect();

  visual_rect.Rect().Move(-layer_offset_.x(), -layer_offset_.y());
  visual_rect.Rect().Inflate(outset_for_raster_effects_);
  AdjustVisualRectBySubpixelOffset(visual_rect.Rect());
  return EnclosingIntRect(visual_rect.Rect());
}

void ChunkToLayerMapper::AdjustVisualRectBySubpixelOffset(
    FloatRect& rect) const {
  // Add back the layer's subpixel accumulation that was excluded from the
  // visual rect by
  // PaintInvalidator::ExcludeCompositedLayerSubpixelAccumulation().
  // The condition below should be kept consistent with that function.
  if (&chunk_state_.Transform() == &layer_state_.Transform())
    rect.Move(visual_rect_subpixel_offset_);
}

}  // namespace blink
