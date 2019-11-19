// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_RASTER_INVALIDATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_RASTER_INVALIDATOR_H_

#include "base/callback.h"
#include "third_party/blink/renderer/platform/graphics/compositing/chunk_to_layer_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/float_clip_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset.h"
#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidation_tracking.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class PaintArtifact;
class IntRect;

class PLATFORM_EXPORT RasterInvalidator {
  USING_FAST_MALLOC(RasterInvalidator);

 public:
  using RasterInvalidationFunction =
      base::RepeatingCallback<void(const IntRect&)>;

  RasterInvalidator(RasterInvalidationFunction raster_invalidation_function)
      : raster_invalidation_function_(std::move(raster_invalidation_function)) {
    DCHECK(!raster_invalidation_function_.is_null());
  }

  void SetTracksRasterInvalidations(bool);
  RasterInvalidationTracking* GetTracking() const {
    return tracking_info_ ? &tracking_info_->tracking : nullptr;
  }

  RasterInvalidationTracking& EnsureTracking();

  // Generate raster invalidations for all of the changed paint chunks and
  // display items in the paint artifact.
  void Generate(scoped_refptr<const PaintArtifact>,
                const gfx::Rect& layer_bounds,
                const PropertyTreeState& layer_state,
                const FloatSize& visual_rect_subpixel_offset = FloatSize(),
                const DisplayItemClient* layer_client = nullptr);

  // Generate raster invalidations for a subset of the paint chunks in the
  // paint artifact.
  void Generate(scoped_refptr<const PaintArtifact>,
                const PaintChunkSubset&,
                const gfx::Rect& layer_bounds,
                const PropertyTreeState& layer_state,
                const FloatSize& visual_rect_subpixel_offset = FloatSize(),
                const DisplayItemClient* layer_client = nullptr);

  const gfx::Rect& LayerBounds() const { return layer_bounds_; }

  size_t ApproximateUnsharedMemoryUsage() const;

  void ClearOldStates();

 private:
  friend class DisplayItemRasterInvalidator;
  friend class RasterInvalidatorTest;

  void UpdateClientDebugNames(const PaintArtifact&, const PaintChunkSubset&);

  struct PaintChunkInfo {
    PaintChunkInfo(const RasterInvalidator& invalidator,
                   const ChunkToLayerMapper& mapper,
                   PaintChunkSubset::Iterator chunk_it)
        : index_in_paint_artifact(chunk_it.OriginalIndex()),
#if DCHECK_IS_ON()
          id(chunk_it->id),
#endif
          bounds_in_layer(invalidator.ClipByLayerBounds(
              mapper.MapVisualRect(chunk_it->bounds))),
          chunk_to_layer_clip(mapper.ClipRect()),
          chunk_to_layer_transform(mapper.Transform()) {
    }

    // The index of the chunk in the PaintArtifact. It may be different from
    // the index of this PaintChunkInfo in paint_chunks_info_ when a subset of
    // the paint chunks is handled by the RasterInvalidator.
    size_t index_in_paint_artifact;

#if DCHECK_IS_ON()
    PaintChunk::Id id;
#endif

    IntRect bounds_in_layer;
    FloatClipRect chunk_to_layer_clip;
    SkMatrix chunk_to_layer_transform;
  };

  void GenerateRasterInvalidations(const PaintArtifact&,
                                   const PaintChunkSubset&,
                                   const PropertyTreeState& layer_state,
                                   const FloatSize& visual_rect_subpixel_offset,
                                   Vector<PaintChunkInfo>& new_chunks_info);

  ALWAYS_INLINE const PaintChunk& GetOldChunk(size_t index) const;
  ALWAYS_INLINE size_t MatchNewChunkToOldChunk(const PaintChunk& new_chunk,
                                               size_t old_index) const;

  ALWAYS_INLINE void IncrementallyInvalidateChunk(
      const PaintChunkInfo& old_chunk_info,
      const PaintChunkInfo& new_chunk_info,
      const DisplayItemClient&);

  // |old_or_new| indicates if |client| is known to be new (alive) and we can
  // get DebugName() directly or should get from |tracking_info_
  // ->old_client_debug_names|.
  enum ClientIsOldOrNew { kClientIsOld, kClientIsNew };
  void AddRasterInvalidation(const IntRect& rect,
                             const DisplayItemClient& client,
                             PaintInvalidationReason reason,
                             ClientIsOldOrNew old_or_new) {
    if (rect.IsEmpty())
      return;
    raster_invalidation_function_.Run(rect);
    if (tracking_info_)
      TrackRasterInvalidation(rect, client, reason, old_or_new);
  }
  void TrackRasterInvalidation(const IntRect&,
                               const DisplayItemClient&,
                               PaintInvalidationReason,
                               ClientIsOldOrNew);

  ALWAYS_INLINE PaintInvalidationReason
  ChunkPropertiesChanged(const RefCountedPropertyTreeState& new_chunk_state,
                         const RefCountedPropertyTreeState& old_chunk_state,
                         const PaintChunkInfo& new_chunk_info,
                         const PaintChunkInfo& old_chunk_info,
                         const PropertyTreeState& layer_state) const;

  // Clip a rect in the layer space by the layer bounds.
  template <typename Rect>
  Rect ClipByLayerBounds(const Rect& r) const {
    return Intersection(
        r, Rect(0, 0, layer_bounds_.width(), layer_bounds_.height()));
  }

  void TrackImplicitFullLayerInvalidation(const DisplayItemClient&);

  RasterInvalidationFunction raster_invalidation_function_;
  gfx::Rect layer_bounds_;
  Vector<PaintChunkInfo> old_paint_chunks_info_;
  scoped_refptr<const PaintArtifact> old_paint_artifact_;

  struct RasterInvalidationTrackingInfo {
    using ClientDebugNamesMap = HashMap<const DisplayItemClient*, String>;
    ClientDebugNamesMap old_client_debug_names;
    RasterInvalidationTracking tracking;
  };
  std::unique_ptr<RasterInvalidationTrackingInfo> tracking_info_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_RASTER_INVALIDATOR_H_
