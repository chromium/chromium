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
  explicit PaintChunker(Vector<PaintChunk>& chunks) { ResetChunks(&chunks); }

  // Finishes current chunks if any, and makes it ready to create chunks into
  // the given vector if not null.
  void ResetChunks(Vector<PaintChunk>*);

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
  void SetWillForceNewChunk(bool force) {
    will_force_new_chunk_ = force;
    next_chunk_id_ = base::nullopt;
  }
  bool WillForceNewChunk() const { return will_force_new_chunk_; }

  void AppendByMoving(PaintChunk&&);

  // Returns true if a new chunk is created.
  bool IncrementDisplayItemIndex(const DisplayItem&);

  // The id will be used when we need to create a new current chunk.
  // Otherwise it's ignored. Returns true if a new chunk is added.
  bool AddHitTestDataToCurrentChunk(const PaintChunk::Id&,
                                    const IntRect&,
                                    TouchAction,
                                    bool blocking_wheel);
  void CreateScrollHitTestChunk(
      const PaintChunk::Id&,
      const TransformPaintPropertyNode* scroll_translation,
      const IntRect&);

  // Returns true if a new chunk is created.
  bool ProcessBackgroundColorCandidate(const PaintChunk::Id& id,
                                       Color color,
                                       uint64_t area);

  // Returns true if a new chunk is created.
  bool EnsureChunk() { return EnsureCurrentChunk(*next_chunk_id_); }

 private:
  // Returns true if a new chunk is created.
  bool EnsureCurrentChunk(const PaintChunk::Id&);

  void FinalizeLastChunkProperties();

  Vector<PaintChunk>* chunks_ = nullptr;

  // The id specified by UpdateCurrentPaintChunkProperties(). If it is not
  // nullopt, we will use it as the id of the next new chunk. Otherwise we will
  // use the id of the first display item of the new chunk as the id.
  // It's cleared when we create a new chunk with the id, or decide not to
  // create a chunk with it (e.g. when properties don't change and we are not
  // forced to create a new chunk).
  base::Optional<PaintChunk::Id> next_chunk_id_;

  PropertyTreeStateOrAlias current_properties_ =
      PropertyTreeState::Uninitialized();

  Region last_chunk_known_to_be_opaque_region_;

  // True when an item forces a new chunk (e.g., foreign display items), and for
  // the item following a forced chunk. PaintController also forces new chunks
  // before and after subsequences by calling ForceNewChunk().
  bool will_force_new_chunk_ = true;

  Color candidate_background_color_ = Color::kTransparent;
  uint64_t candidate_background_area_ = 0;

  DISALLOW_COPY_AND_ASSIGN(PaintChunker);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CHUNKER_H_
