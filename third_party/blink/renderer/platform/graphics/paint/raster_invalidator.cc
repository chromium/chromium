// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidator.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "third_party/blink/renderer/platform/graphics/compositing/paint_chunks_to_cc_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_raster_invalidator.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

void RasterInvalidator::Trace(Visitor* visitor) const {
  visitor->Trace(layer_state_);
  visitor->Trace(current_paint_artifact_);
  visitor->Trace(old_paint_artifact_);
  visitor->Trace(tracking_);
}

void RasterInvalidator::SetTracksRasterInvalidations(bool should_track) {
  if (should_track) {
    if (!tracking_) {
      tracking_ = MakeGarbageCollected<RasterInvalidationTracking>();
    }
    tracking_->ClearInvalidations();
  } else if (!RasterInvalidationTracking::ShouldAlwaysTrack()) {
    tracking_ = nullptr;
  } else if (tracking_) {
    tracking_->ClearInvalidations();
  }
}

const PaintChunk& RasterInvalidator::GetOldChunk(wtf_size_t index) const {
  DCHECK(old_paint_artifact_);
  const auto& old_chunk_info = old_paint_chunks_info_[index];
  const auto& old_chunk =
      old_paint_artifact_
          ->GetPaintChunks()[old_chunk_info.index_in_paint_artifact];
#if DCHECK_IS_ON()
  DCHECK_EQ(old_chunk.id, old_chunk_info.id);
#endif
  return old_chunk;
}

wtf_size_t RasterInvalidator::MatchNewChunkToOldChunk(
    const PaintChunk& new_chunk,
    wtf_size_t old_index) const {
  if (!new_chunk.CanMatchOldChunk())
    return kNotFound;

  for (wtf_size_t i = old_index; i < old_paint_chunks_info_.size(); i++) {
    if (new_chunk.Matches(GetOldChunk(i)))
      return i;
  }
  return kNotFound;
}

PaintInvalidationReason RasterInvalidator::ChunkPropertiesChanged(
    const PaintChunk& new_chunk,
    const PaintChunk& old_chunk,
    const PaintChunkInfo& new_chunk_info,
    const PaintChunkInfo& old_chunk_info,
    const PropertyTreeState& layer_state,
    const float absolute_translation_tolerance,
    const float other_transform_tolerance) const {
  if (new_chunk.effectively_invisible != old_chunk.effectively_invisible)
    return PaintInvalidationReason::kPaintProperty;

  // Special case for transform changes because we may create or delete some
  // transform nodes when no raster invalidation is needed. For example, when
  // a composited layer previously not transformed now gets transformed.
  // Check for real accumulated transform change instead.
  if (!new_chunk_info.chunk_to_layer_transform.ApproximatelyEqual(
          old_chunk_info.chunk_to_layer_transform,
          absolute_translation_tolerance, other_transform_tolerance,
          other_transform_tolerance)) {
    return PaintInvalidationReason::kPaintProperty;
  }

  // Treat the chunk property as changed if the effect node pointer is
  // different, or the effect node's value changed between the layer state and
  // the chunk state.
  const auto& new_chunk_state = new_chunk.properties;
  bool clip_node_is_different = false;
  bool effect_node_is_different = false;
  if (!new_chunk.is_moved_from_cached_subsequence) {
    clip_node_is_different =
        &new_chunk_state.Clip() != &old_chunk.properties.Clip();
    effect_node_is_different =
        &new_chunk_state.Effect() != &old_chunk.properties.Effect();
  }
  if (effect_node_is_different ||
      new_chunk_state.Effect().Changed(
          PaintPropertyChangeType::kChangedOnlySimpleValues, layer_state,
          &new_chunk_state.Transform())) {
    return PaintInvalidationReason::kPaintProperty;
  }

  // Check for accumulated clip rect change, if the clip rects are tight.
  if (new_chunk_info.chunk_to_layer_clip.IsTight() &&
      old_chunk_info.chunk_to_layer_clip.IsTight()) {
    gfx::RectF new_clip_rect = new_chunk_info.chunk_to_layer_clip.Rect();
    gfx::RectF old_clip_rect = old_chunk_info.chunk_to_layer_clip.Rect();
    if (new_clip_rect == old_clip_rect)
      return PaintInvalidationReason::kNone;
    // Ignore differences out of the current layer bounds.
    gfx::RectF new_clip_in_layer_bounds = ClipByLayerBounds(new_clip_rect);
    gfx::RectF old_clip_in_layer_bounds = ClipByLayerBounds(old_clip_rect);
    if (new_clip_in_layer_bounds == old_clip_in_layer_bounds)
      return PaintInvalidationReason::kNone;

    // Clip changed and may have visual effect, so we need raster invalidation.
    if (!new_clip_in_layer_bounds.Contains(
            gfx::RectF(new_chunk_info.bounds_in_layer)) ||
        !old_clip_in_layer_bounds.Contains(
            gfx::RectF(old_chunk_info.bounds_in_layer))) {
      // If the chunk is not fully covered by the clip rect, we have to do full
      // invalidation instead of incremental because the delta parts of the
      // layer bounds may not cover all changes caused by the clip change.
      // This can happen because of pixel snapping, raster effect outset, etc.
      return PaintInvalidationReason::kPaintProperty;
    }
    // Otherwise we just invalidate the delta parts of the layer bounds.
    return PaintInvalidationReason::kIncremental;
  }

  // Otherwise treat the chunk property as changed if the clip node pointer is
  // different, or the clip node's value changed between the layer state and the
  // chunk state.
  if (clip_node_is_different ||
      new_chunk_state.Clip().Changed(
          PaintPropertyChangeType::kChangedOnlySimpleValues, layer_state,
          &new_chunk_state.Transform()))
    return PaintInvalidationReason::kPaintProperty;

  return PaintInvalidationReason::kNone;
}

static bool ShouldSkipForRasterInvalidation(
    const PaintChunkIterator& chunk_it) {
  if (!chunk_it->DrawsContent())
    return true;

  // Foreign layers take care of raster invalidation by themselves.
  if (DisplayItem::IsForeignLayerType(chunk_it->id.type))
    return true;

  return false;
}

static bool ScrollbarNeedsUpdateDisplay(const PaintChunkIterator& chunk_it) {
  if (chunk_it->size() != 1) {
    return false;
  }
  if (auto* scrollbar =
          DynamicTo<ScrollbarDisplayItem>(*chunk_it.DisplayItems().begin())) {
    return scrollbar->NeedsUpdateDisplay();
  }
  return false;
}

// Generates raster invalidations by checking changes (appearing, disappearing,
// reordering, property changes) of chunks. The logic is similar to
// PaintController::GenerateRasterInvalidations(). The complexity is between
// O(n) and O(m*n) where m and n are the numbers of old and new chunks,
// respectively. Normally both m and n are small numbers. The best case is that
// all old chunks have matching new chunks in the same order. The worst case is
// that no matching chunks except the first one (which always matches otherwise
// we won't reuse the RasterInvalidator), which is rare. In
// common cases that most of the chunks can be matched in-order, the complexity
// is slightly larger than O(n).
void RasterInvalidator::GenerateRasterInvalidations(
    const PaintChunkSubset& new_chunks,
    bool layer_offset_or_state_changed,
    bool layer_effect_changed,
    Vector<PaintChunkInfo>& new_chunks_info) {
  ChunkToLayerMapper mapper(PropertyTreeState(layer_state_), layer_offset_);
  Vector<bool> old_chunks_matched;
  old_chunks_matched.resize(old_paint_chunks_info_.size());
  wtf_size_t old_index = 0;

  const float absolute_translation_tolerance = 1e-2f;
  const float other_transform_tolerance = 1e-4f;

  for (auto it = new_chunks.begin(); it != new_chunks.end(); ++it) {
    if (ShouldSkipForRasterInvalidation(it))
      continue;

    const auto& new_chunk = *it;
    auto matched_old_index = MatchNewChunkToOldChunk(new_chunk, old_index);
    if (matched_old_index == kNotFound) {
      // The new chunk doesn't match any old chunk.
      mapper.SwitchToChunk(new_chunk);
      auto& new_chunk_info = new_chunks_info.emplace_back(*this, mapper, it);
      AddRasterInvalidation(
          new_chunk_info.bounds_in_layer, new_chunk.id.client_id,
          new_chunk.is_cacheable ? PaintInvalidationReason::kChunkAppeared
                                 : PaintInvalidationReason::kChunkUncacheable,
          kClientIsNew);
      continue;
    }

    DCHECK(!old_chunks_matched[matched_old_index]);
    old_chunks_matched[matched_old_index] = true;

    auto& old_chunk_info = old_paint_chunks_info_[matched_old_index];
    const auto& old_chunk = GetOldChunk(matched_old_index);
    // Clip the old chunk bounds by the new layer bounds.
    old_chunk_info.bounds_in_layer =
        ClipByLayerBounds(old_chunk_info.bounds_in_layer);

    auto reason = PaintInvalidationReason::kNone;
    if (ScrollbarNeedsUpdateDisplay(it)) {
      reason = PaintInvalidationReason::kScrollControl;
    }

    // No need to invalidate if the chunk is moved from cached subsequence and
    // its paint properties didn't change relative to the layer.
    if (!layer_offset_or_state_changed &&
        reason == PaintInvalidationReason::kNone &&
        new_chunk.is_moved_from_cached_subsequence &&
        !new_chunk.properties.Changed(
            PaintPropertyChangeType::kChangedOnlySimpleValues,
            PropertyTreeState(layer_state_))) {
      new_chunks_info.emplace_back(old_chunk_info, it);
    } else {
      mapper.SwitchToChunk(new_chunk);
      auto& new_chunk_info = new_chunks_info.emplace_back(*this, mapper, it);

      if (reason == PaintInvalidationReason::kNone) {
        if (layer_effect_changed) {
          // Because of DecompositeEffect, the layer's effect may have changed
          // even if the chunk's didn't.
          reason = PaintInvalidationReason::kPaintProperty;
        } else {
          reason = ChunkPropertiesChanged(
              new_chunk, old_chunk, new_chunk_info, old_chunk_info,
              PropertyTreeState(layer_state_), absolute_translation_tolerance,
              other_transform_tolerance);
        }
      }

      if (IsFullPaintInvalidationReason(reason)) {
        // Invalidate both old and new bounds of the chunk if the chunk's paint
        // properties changed, or is moved backward and may expose area that was
        // previously covered by it.
        AddRasterInvalidation(old_chunk_info.bounds_in_layer,
                              new_chunk.id.client_id, reason, kClientIsNew);
        if (old_chunk_info.bounds_in_layer != new_chunk_info.bounds_in_layer) {
          AddRasterInvalidation(new_chunk_info.bounds_in_layer,
                                new_chunk.id.client_id, reason, kClientIsNew);
        }
        // Ignore the display item raster invalidations because we have fully
        // invalidated the chunk.
      } else {
        // We may have ignored tiny changes of transform, in which case we
        // should use the old chunk_to_layer_transform for later comparison to
        // correctly invalidate animating transform in tiny increments when the
        // accumulated change exceeds the tolerance.
        new_chunk_info.chunk_to_layer_transform =
            old_chunk_info.chunk_to_layer_transform;

        if (reason == PaintInvalidationReason::kIncremental) {
          IncrementallyInvalidateChunk(old_chunk_info, new_chunk_info,
                                       new_chunk.id.client_id);
        }

        if (&new_chunks.GetPaintArtifact() != old_paint_artifact_ &&
            !new_chunk.is_moved_from_cached_subsequence) {
          DisplayItemRasterInvalidator(
              *this,
              old_paint_artifact_->DisplayItemsInChunk(
                  old_chunk_info.index_in_paint_artifact),
              it.DisplayItems(), mapper)
              .Generate();
        }
      }
    }

    old_index = matched_old_index + 1;
  }

  // Invalidate remaining unmatched (disappeared or uncacheable) old chunks.
  for (wtf_size_t i = 0; i < old_paint_chunks_info_.size(); ++i) {
    if (old_chunks_matched[i])
      continue;

    const auto& old_chunk = GetOldChunk(i);
    auto reason = old_chunk.is_cacheable
                      ? PaintInvalidationReason::kChunkDisappeared
                      : PaintInvalidationReason::kChunkUncacheable;
    AddRasterInvalidation(old_paint_chunks_info_[i].bounds_in_layer,
                          old_chunk.id.client_id, reason, kClientIsOld);
  }
}

void RasterInvalidator::IncrementallyInvalidateChunk(
    const PaintChunkInfo& old_chunk_info,
    const PaintChunkInfo& new_chunk_info,
    DisplayItemClientId client_id) {
  SkRegion diff(gfx::RectToSkIRect(old_chunk_info.bounds_in_layer));
  diff.op(gfx::RectToSkIRect(new_chunk_info.bounds_in_layer),
          SkRegion::kXOR_Op);
  for (SkRegion::Iterator it(diff); !it.done(); it.next()) {
    AddRasterInvalidation(gfx::SkIRectToRect(it.rect()), client_id,
                          PaintInvalidationReason::kIncremental, kClientIsNew);
  }
}

void RasterInvalidator::TrackRasterInvalidation(const gfx::Rect& rect,
                                                DisplayItemClientId client_id,
                                                PaintInvalidationReason reason,
                                                ClientIsOldOrNew old_or_new) {
  DCHECK(tracking_);
  String debug_name = old_or_new == kClientIsOld
                          ? old_paint_artifact_->ClientDebugName(client_id)
                          : current_paint_artifact_->ClientDebugName(client_id);
  tracking_->AddInvalidation(client_id, debug_name, rect, reason);
}

RasterInvalidationTracking& RasterInvalidator::EnsureTracking() {
  if (!tracking_) {
    tracking_ = MakeGarbageCollected<RasterInvalidationTracking>();
  }
  return *tracking_;
}

void RasterInvalidator::Generate(
    const PaintChunkSubset& new_chunks,
    const gfx::Vector2dF& layer_offset,
    const gfx::Size& layer_bounds,
    const PropertyTreeState& layer_state) {
  if (RasterInvalidationTracking::ShouldAlwaysTrack())
    EnsureTracking();

  bool layer_offset_or_state_changed =
      layer_offset_ != layer_offset || layer_state_ != layer_state;
  bool layer_effect_changed = &layer_state_.Effect() != &layer_state.Effect();
  bool layer_bounds_was_empty = layer_bounds_.IsEmpty();
  layer_offset_ = layer_offset;
  layer_bounds_ = layer_bounds;
  layer_state_ = layer_state;
  current_paint_artifact_ = &new_chunks.GetPaintArtifact();

  Vector<PaintChunkInfo> new_chunks_info;
  new_chunks_info.reserve(new_chunks.size());

  if (layer_bounds_was_empty || layer_bounds_.IsEmpty()) {
    // Fast path if either the old bounds or the new bounds is empty. We still
    // need to update new_chunks_info for the next cycle.
    ChunkToLayerMapper mapper(layer_state, layer_offset);
    for (auto it = new_chunks.begin(); it != new_chunks.end(); ++it) {
      if (ShouldSkipForRasterInvalidation(it))
        continue;
      mapper.SwitchToChunk(*it);
      new_chunks_info.emplace_back(*this, mapper, it);
    }

    if (!layer_bounds.IsEmpty() && !new_chunks.IsEmpty()) {
      AddRasterInvalidation(gfx::Rect(layer_bounds),
                            new_chunks.begin()->id.client_id,
                            PaintInvalidationReason::kFullLayer, kClientIsNew);
    }
  } else {
    GenerateRasterInvalidations(new_chunks, layer_offset_or_state_changed,
                                layer_effect_changed, new_chunks_info);
  }

  old_paint_chunks_info_ = std::move(new_chunks_info);
  old_paint_artifact_ = &new_chunks.GetPaintArtifact();
  current_paint_artifact_ = nullptr;
}

void RasterInvalidator::SetOldPaintArtifact(
    const PaintArtifact& old_paint_artifact) {
  old_paint_artifact_ = &old_paint_artifact;
}

size_t RasterInvalidator::ApproximateUnsharedMemoryUsage() const {
  return sizeof(*this) +
         old_paint_chunks_info_.capacity() * sizeof(PaintChunkInfo);
}

void RasterInvalidator::ClearOldStates() {
  old_paint_artifact_ = nullptr;
  old_paint_chunks_info_.clear();
  layer_offset_ = gfx::Vector2dF();
  layer_bounds_ = gfx::Size();
}

}  // namespace blink
