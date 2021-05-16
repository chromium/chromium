// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_SCOPED_PAINT_CHUNK_HINT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_SCOPED_PAINT_CHUNK_HINT_H_

#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"

namespace blink {

// Hints for new paint chunks for a scope during paint. Will create new paint
// chunks if we create any display items in the paint scope and
// - paint chunk properties are different from the current chunk, or
// - If the client's visual rect isn't fully contained by the current chunk's
//   bounds. This is a heuristic to create only paint chunks that are meaningful
//   to CompositeAfterPaint layerization, i.e. avoid unnecessary forced paint
//   chunks that will definitely be merged into the previous paint chunk.
// Hinted paint chunks are never required for correctness and rendering should
// be the same without this class. These hints are for performance: by causing
// additional paint chunks (while avoiding some unnecessary ones) with explicit
// ids, we can improve raster invalidation and layerization.
class ScopedPaintChunkHint {
  STACK_ALLOCATED();

 public:
  ScopedPaintChunkHint(PaintController& paint_controller,
                       const DisplayItemClient& client,
                       DisplayItem::Type type,
                       const IntRect& visual_rect)
      : ScopedPaintChunkHint(paint_controller,
                             paint_controller.CurrentPaintChunkProperties(),
                             client,
                             type,
                             visual_rect) {}

  ScopedPaintChunkHint(PaintController& paint_controller,
                       const PropertyTreeStateOrAlias& properties,
                       const DisplayItemClient& client,
                       DisplayItem::Type type,
                       const IntRect& visual_rect)
      : paint_controller_(paint_controller),
        previous_num_chunks_(paint_controller_.NumNewChunks()),
        previous_force_new_chunk_(paint_controller_.WillForceNewChunk()) {
    if (!previous_force_new_chunk_ && previous_num_chunks_ &&
        !paint_controller_.LastChunkBounds().Contains(visual_rect))
      paint_controller_.SetWillForceNewChunk(true);
    // This is after SetForceNewChunk(true) so that the possible new chunk will
    // use the specified id.
    paint_chunk_properties_.emplace(paint_controller, properties, client, type);
  }

  ~ScopedPaintChunkHint() {
    paint_chunk_properties_ = absl::nullopt;
    if (!HasCreatedPaintChunk())
      paint_controller_.SetWillForceNewChunk(previous_force_new_chunk_);
  }

  bool HasCreatedPaintChunk() const {
    DCHECK_GE(paint_controller_.NumNewChunks(), previous_num_chunks_);
    return paint_controller_.NumNewChunks() > previous_num_chunks_;
  }

 private:
  PaintController& paint_controller_;
  // This is actually always emplaced, but is wrapped in absl::optional<> to
  // control its lifetime related to SetForceNewChunk().
  absl::optional<ScopedPaintChunkProperties> paint_chunk_properties_;
  wtf_size_t previous_num_chunks_;
  bool previous_force_new_chunk_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_SCOPED_PAINT_CHUNK_HINT_H_
