// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CHUNKER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CHUNKER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "third_party/blink/renderer/platform/geometry/region.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// Accepts information about changes to chunk properties as drawings are
// accumulated, and produces a series of paint chunks: contiguous ranges of the
// display list with identical properties.
class PLATFORM_EXPORT PaintChunker final {
  DISALLOW_NEW();

 public:
  PaintChunker();
  ~PaintChunker();

#if DCHECK_IS_ON()
  bool IsInInitialState() const;
#endif

  const PropertyTreeStateOrAlias& CurrentPaintChunkProperties() const {
    return current_properties_;
  }
  void UpdateCurrentPaintChunkProperties(const PaintChunk::Id*,
                                         const PropertyTreeStateOrAlias&);

  // Sets the forcing new chunk status on or off. If the status is on, even the
  // properties haven't change, we'll force a new paint chunk for the next
  // display item and then automatically resets the status. Some special display
  // item (e.g. ForeignLayerDisplayItem) also automatically sets the status on
  // before and after the item to force a dedicated paint chunk.
  void SetForceNewChunk(bool force) {
    force_new_chunk_ = force;
    next_chunk_id_ = base::nullopt;
  }
  bool WillForceNewChunk() const {
    return force_new_chunk_ || chunks_.IsEmpty();
  }

  void AppendByMoving(PaintChunk&&);

  // Returns true if a new chunk is created.
  bool IncrementDisplayItemIndex(const DisplayItem&);

  const Vector<PaintChunk>& PaintChunks() const { return chunks_; }
  wtf_size_t size() const { return chunks_.size(); }

  PaintChunk& LastChunk() { return chunks_.back(); }
  const PaintChunk& LastChunk() const { return chunks_.back(); }

  // The id will be used when we need to create a new current chunk.
  // Otherwise it's ignored.
  void AddHitTestDataToCurrentChunk(const PaintChunk::Id&,
                                    const IntRect&,
                                    TouchAction);
  void CreateScrollHitTestChunk(
      const PaintChunk::Id&,
      const TransformPaintPropertyNode* scroll_translation,
      const IntRect&);

  void ProcessBackgroundColorCandidate(const PaintChunk::Id& id,
                                       Color color,
                                       uint64_t area);
  void EnsureChunk() { EnsureCurrentChunk(*next_chunk_id_); }

  // Releases the generated paint chunk list and raster invalidations and
  // resets the state of this object.
  Vector<PaintChunk> ReleasePaintChunks();

 private:
  PaintChunk& EnsureCurrentChunk(const PaintChunk::Id&);
  void FinalizeLastChunkProperties();

  Vector<PaintChunk> chunks_;

  // The id specified by UpdateCurrentPaintChunkProperties(). If it is not
  // nullopt, we will use it as the id of the next new chunk. Otherwise we will
  // use the id of the first display item of the new chunk as the id.
  // It's cleared when we create a new chunk with the id, or decide not to
  // create a chunk with it (e.g. when properties don't change and we are not
  // forced to create a new chunk).
  base::Optional<PaintChunk::Id> next_chunk_id_;

  PropertyTreeStateOrAlias current_properties_;

  Region last_chunk_known_to_be_opaque_region_;

  // True when an item forces a new chunk (e.g., foreign display items), and for
  // the item following a forced chunk. PaintController also forces new chunks
  // before and after subsequences by calling ForceNewChunk().
  bool force_new_chunk_;

  Color candidate_background_color_ = Color::kTransparent;
  uint64_t candidate_background_area_ = 0;

  DISALLOW_COPY_AND_ASSIGN(PaintChunker);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CHUNKER_H_
