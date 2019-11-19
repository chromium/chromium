// Copyright 2015 The Chromium Authors. All rights reserved.
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

namespace blink {

void RasterInvalidator::UpdateClientDebugNames(
    const PaintArtifact& paint_artifact,
    const PaintChunkSubset& paint_chunks) {
  DCHECK(tracking_info_);
  auto& debug_names = tracking_info_->old_client_debug_names;
  debug_names.clear();
  for (const auto& chunk : paint_chunks) {
    debug_names.insert(&chunk.id.client, chunk.id.client.DebugName());
    for (const auto& item :
         paint_artifact.GetDisplayItemList().ItemsInPaintChunk(chunk))
      debug_names.insert(&item.Client(), item.Client().DebugName());
  }
}

void RasterInvalidator::SetTracksRasterInvalidations(bool should_track) {
  if (should_track) {
    if (!tracking_info_)
      tracking_info_ = std::make_unique<RasterInvalidationTrackingInfo>();
    tracking_info_->tracking.ClearInvalidations();

    // This is called just after a full document cycle update, so all clients in
    // old_paint_artifact_ should be still alive.
    if (old_paint_artifact_) {
      UpdateClientDebugNames(*old_paint_artifact_,
                             old_paint_artifact_->PaintChunks());
    }
  } else if (!RasterInvalidationTracking::ShouldAlwaysTrack()) {
    tracking_info_ = nullptr;
  } else if (tracking_info_) {
    tracking_info_->tracking.ClearInvalidations();
  }
}

const PaintChunk& RasterInvalidator::GetOldChunk(size_t index) const {
  DCHECK(old_paint_artifact_);
  const auto& old_chunk_info = old_paint_chunks_info_[index];
  const auto& old_chunk =
      old_paint_artifact_
          ->PaintChunks()[old_chunk_info.index_in_paint_artifact];
#if DCHECK_IS_ON()
  DCHECK(old_chunk.id == old_chunk_info.id);
#endif
  return old_chunk;
}

size_t RasterInvalidator::MatchNewChunkToOldChunk(const PaintChunk& new_chunk,
                                                  size_t old_index) const {
  for (size_t i = old_index; i < old_paint_chunks_info_.size(); i++) {
    if (new_chunk.Matches(GetOldChunk(i)))
      return i;
  }
  for (size_t i = 0; i < old_index; i++) {
    if (new_chunk.Matches(GetOldChunk(i)))
      return i;
  }
  return kNotFound;
}

static bool ApproximatelyEqual(const SkMatrix& a, const SkMatrix& b) {
  static constexpr float kTolerance = 1e-5f;
  for (int i = 0; i < 9; i++) {
    auto difference = std::abs(a[i] - b[i]);
    // Check for absolute difference.
    if (difference > kTolerance)
      return false;
    // For scale components, also check for relative difference.
    if ((i == 0 || i == 4 || i == 9) &&
        difference > (std::abs(a[i]) + std::abs(b[i])) * kTolerance)
      return false;
  }
  return true;
}

PaintInvalidationReason RasterInvalidator::ChunkPropertiesChanged(
    const RefCountedPropertyTreeState& new_chunk_state,
    const RefCountedPropertyTreeState& old_chunk_state,
    const PaintChunkInfo& new_chunk_info,
    const PaintChunkInfo& old_chunk_info,
    const PropertyTreeState& layer_state) const {
  // Special case for transform changes because we may create or delete some
  // transform nodes when no raster invalidation is needed. For example, when
  // a composited layer previously not transformed now gets transformed.
  // Check for real accumulated transform change instead.
  if (!ApproximatelyEqual(new_chunk_info.chunk_to_layer_transform,
                          old_chunk_info.chunk_to_layer_transform))
    return PaintInvalidationReason::kPaintProperty;

  // Treat the chunk property as changed if the effect node pointer is
  // different, or the effect node's value changed between the layer state and
  // the chunk state.
  if (&new_chunk_state.Effect() != &old_chunk_state.Effect() ||
      new_chunk_state.Effect().Changed(
          PaintPropertyChangeType::kChangedOnlySimpleValues, layer_state,
          &new_chunk_state.Transform()))
    return PaintInvalidationReason::kPaintProperty;

  // Check for accumulated clip rect change, if the clip rects are tight.
  if (new_chunk_info.chunk_to_layer_clip.IsTight() &&
      old_chunk_info.chunk_to_layer_clip.IsTight()) {
    const auto& new_clip_rect = new_chunk_info.chunk_to_layer_clip.Rect();
    const auto& old_clip_rect = old_chunk_info.chunk_to_layer_clip.Rect();
    if (new_clip_rect == old_clip_rect)
      return PaintInvalidationReason::kNone;
    // Ignore differences out of the current layer bounds.
    auto new_clip_in_layer_bounds = ClipByLayerBounds(new_clip_rect);
    auto old_clip_in_layer_bounds = ClipByLayerBounds(old_clip_rect);
    if (new_clip_in_layer_bounds == old_clip_in_layer_bounds)
      return PaintInvalidationReason::kNone;

    // Clip changed and may have visual effect, so we need raster invalidation.
    if (!new_clip_in_layer_bounds.Contains(new_chunk_info.bounds_in_layer) ||
        !old_clip_in_layer_bounds.Contains(old_chunk_info.bounds_in_layer)) {
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
  if (&new_chunk_state.Clip() != &old_chunk_state.Clip() ||
      new_chunk_state.Clip().Changed(
          PaintPropertyChangeType::kChangedOnlySimpleValues, layer_state,
          &new_chunk_state.Transform()))
    return PaintInvalidationReason::kPaintProperty;

  return PaintInvalidationReason::kNone;
}

// Generates raster invalidations by checking changes (appearing, disappearing,
// reordering, property changes) of chunks. The logic is similar to
// PaintController::GenerateRasterInvalidations(). The complexity is between
// O(n) and O(m*n) where m and n are the numbers of old and new chunks,
// respectively. Normally both m and n are small numbers. The best caseis that
// all old chunks have matching new chunks in the same order. The worst case is
// that no matching chunks except the first one (which always matches otherwise
// we won't reuse the RasterInvalidator), which is rare. In
// common cases that most of the chunks can be matched in-order, the complexity
// is slightly larger than O(n).
void RasterInvalidator::GenerateRasterInvalidations(
    const PaintArtifact& new_paint_artifact,
    const PaintChunkSubset& new_chunks,
    const PropertyTreeState& layer_state,
    const FloatSize& visual_rect_subpixel_offset,
    Vector<PaintChunkInfo>& new_chunks_info) {
  ChunkToLayerMapper mapper(layer_state, layer_bounds_.OffsetFromOrigin(),
                            visual_rect_subpixel_offset);
  Vector<bool> old_chunks_matched;
  old_chunks_matched.resize(old_paint_chunks_info_.size());
  size_t old_index = 0;
  size_t max_matched_old_index = 0;
  for (auto it = new_chunks.begin(); it != new_chunks.end(); ++it) {
    const auto& new_chunk = *it;
    mapper.SwitchToChunk(new_chunk);
    auto& new_chunk_info = new_chunks_info.emplace_back(*this, mapper, it);

    // Foreign layers take care of raster invalidation by themselves.
    if (DisplayItem::IsForeignLayerType(new_chunk.id.type))
      continue;

    if (!new_chunk.is_cacheable) {
      AddRasterInvalidation(new_chunk_info.bounds_in_layer, new_chunk.id.client,
                            PaintInvalidationReason::kChunkUncacheable,
                            kClientIsNew);
      continue;
    }

    size_t matched_old_index = MatchNewChunkToOldChunk(new_chunk, old_index);
    if (matched_old_index == kNotFound) {
      // The new chunk doesn't match any old chunk.
      AddRasterInvalidation(new_chunk_info.bounds_in_layer, new_chunk.id.client,
                            PaintInvalidationReason::kChunkAppeared,
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

    PaintInvalidationReason reason =
        matched_old_index < max_matched_old_index
            ? PaintInvalidationReason::kChunkReordered
            : ChunkPropertiesChanged(new_chunk.properties, old_chunk.properties,
                                     new_chunk_info, old_chunk_info,
                                     layer_state);

    if (IsFullPaintInvalidationReason(reason)) {
      // Invalidate both old and new bounds of the chunk if the chunk's paint
      // properties changed, or is moved backward and may expose area that was
      // previously covered by it.
      AddRasterInvalidation(old_chunk_info.bounds_in_layer, new_chunk.id.client,
                            reason, kClientIsNew);
      if (old_chunk_info.bounds_in_layer != new_chunk_info.bounds_in_layer) {
        AddRasterInvalidation(new_chunk_info.bounds_in_layer,
                              new_chunk.id.client, reason, kClientIsNew);
      }
      // Ignore the display item raster invalidations because we have fully
      // invalidated the chunk.
    } else {
      // We may have ignored tiny changes of transform, in which case we should
      // use the old chunk_to_layer_transform for later comparison to correctly
      // invalidate animating transform in tiny increments when the accumulated
      // change exceeds the tolerance.
      new_chunk_info.chunk_to_layer_transform =
          old_chunk_info.chunk_to_layer_transform;

      if (reason == PaintInvalidationReason::kIncremental) {
        IncrementallyInvalidateChunk(old_chunk_info, new_chunk_info,
                                     new_chunk.id.client);
      }

      if (&new_paint_artifact != old_paint_artifact_) {
        DisplayItemRasterInvalidator(*this, *old_paint_artifact_,
                                     new_paint_artifact, old_chunk, new_chunk,
                                     mapper)
            .Generate();
      }
    }

    old_index = matched_old_index + 1;
    if (old_index == old_paint_chunks_info_.size())
      old_index = 0;
    max_matched_old_index = std::max(max_matched_old_index, matched_old_index);
  }

  // Invalidate remaining unmatched (disappeared or uncacheable) old chunks.
  for (size_t i = 0; i < old_paint_chunks_info_.size(); ++i) {
    if (old_chunks_matched[i])
      continue;

    const auto& old_chunk = GetOldChunk(i);
    auto reason = old_chunk.is_cacheable
                      ? PaintInvalidationReason::kChunkDisappeared
                      : PaintInvalidationReason::kChunkUncacheable;
    AddRasterInvalidation(old_paint_chunks_info_[i].bounds_in_layer,
                          old_chunk.id.client, reason, kClientIsOld);
  }
}

void RasterInvalidator::IncrementallyInvalidateChunk(
    const PaintChunkInfo& old_chunk_info,
    const PaintChunkInfo& new_chunk_info,
    const DisplayItemClient& client) {
  SkRegion diff(old_chunk_info.bounds_in_layer);
  diff.op(new_chunk_info.bounds_in_layer, SkRegion::kXOR_Op);
  for (SkRegion::Iterator it(diff); !it.done(); it.next()) {
    AddRasterInvalidation(IntRect(it.rect()), client,
                          PaintInvalidationReason::kIncremental, kClientIsNew);
  }
}

void RasterInvalidator::TrackRasterInvalidation(const IntRect& rect,
                                                const DisplayItemClient& client,
                                                PaintInvalidationReason reason,
                                                ClientIsOldOrNew old_or_new) {
  DCHECK(tracking_info_);
  String debug_name = old_or_new == kClientIsOld
                          ? tracking_info_->old_client_debug_names.at(&client)
                          : client.DebugName();
  tracking_info_->tracking.AddInvalidation(&client, debug_name, rect, reason);
}

RasterInvalidationTracking& RasterInvalidator::EnsureTracking() {
  if (!tracking_info_)
    tracking_info_ = std::make_unique<RasterInvalidationTrackingInfo>();
  return tracking_info_->tracking;
}

void RasterInvalidator::Generate(
    scoped_refptr<const PaintArtifact> new_paint_artifact,
    const gfx::Rect& layer_bounds,
    const PropertyTreeState& layer_state,
    const FloatSize& visual_rect_subpixel_offset,
    const DisplayItemClient* layer_client) {
  Generate(new_paint_artifact, new_paint_artifact->PaintChunks(), layer_bounds,
           layer_state, visual_rect_subpixel_offset, layer_client);
}

void RasterInvalidator::Generate(
    scoped_refptr<const PaintArtifact> new_paint_artifact,
    const PaintChunkSubset& paint_chunks,
    const gfx::Rect& layer_bounds,
    const PropertyTreeState& layer_state,
    const FloatSize& visual_rect_subpixel_offset,
    const DisplayItemClient* layer_client) {
  if (RasterInvalidationTracking::ShouldAlwaysTrack())
    EnsureTracking();

  bool layer_bounds_was_empty = layer_bounds_.IsEmpty();
  layer_bounds_ = layer_bounds;

  Vector<PaintChunkInfo> new_chunks_info;
  new_chunks_info.ReserveCapacity(paint_chunks.size());

  if (layer_bounds_was_empty || layer_bounds_.IsEmpty()) {
    // No raster invalidation is needed if either the old bounds or the new
    // bounds is empty, but we still need to update new_chunks_info for the
    // next cycle.
    ChunkToLayerMapper mapper(layer_state, layer_bounds.OffsetFromOrigin(),
                              visual_rect_subpixel_offset);
    for (auto it = paint_chunks.begin(); it != paint_chunks.end(); ++it) {
      mapper.SwitchToChunk(*it);
      new_chunks_info.emplace_back(*this, mapper, it);
    }

    if (tracking_info_ && layer_bounds_was_empty && !layer_bounds.IsEmpty() &&
        paint_chunks.size()) {
      TrackImplicitFullLayerInvalidation(
          layer_client ? *layer_client : paint_chunks[0].id.client);
    }
  } else {
    GenerateRasterInvalidations(*new_paint_artifact, paint_chunks, layer_state,
                                visual_rect_subpixel_offset, new_chunks_info);
  }

  if (tracking_info_)
    UpdateClientDebugNames(*new_paint_artifact, paint_chunks);

  old_paint_chunks_info_ = std::move(new_chunks_info);
  old_paint_artifact_ = std::move(new_paint_artifact);
}

void RasterInvalidator::TrackImplicitFullLayerInvalidation(
    const DisplayItemClient& layer_client) {
  DCHECK(tracking_info_);

  // Early return if we have already invalidated the whole layer.
  IntRect full_layer_rect(0, 0, layer_bounds_.width(), layer_bounds_.height());
  for (const auto& invalidation : tracking_info_->tracking.Invalidations()) {
    if (invalidation.rect.Contains(full_layer_rect))
      return;
  }

  tracking_info_->tracking.AddInvalidation(
      &layer_client, layer_client.DebugName(), full_layer_rect,
      PaintInvalidationReason::kFullLayer);
}

size_t RasterInvalidator::ApproximateUnsharedMemoryUsage() const {
  return sizeof(*this) +
         old_paint_chunks_info_.capacity() * sizeof(PaintChunkInfo);
}

void RasterInvalidator::ClearOldStates() {
  old_paint_artifact_ = nullptr;
  old_paint_chunks_info_.clear();
  layer_bounds_ = gfx::Rect();
}

}  // namespace blink
