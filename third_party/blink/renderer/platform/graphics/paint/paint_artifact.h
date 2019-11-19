// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_ARTIFACT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_ARTIFACT_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_list.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace cc {
class PaintCanvas;
}

namespace blink {
class GraphicsContext;
class PaintChunkSubset;

// The output of painting, consisting of display item list (in DisplayItemList)
// and paint chunks.
//
// Display item list and paint chunks not only represent the output of the
// current painting, but also serve as cache of individual display items and
// paint chunks for later paintings as long as the display items and chunks are
// valid.
//
// It represents a particular state of the world, and should be immutable
// (const) to most of its users.
//
// Unless its dangerous accessors are used, it promises to be in a reasonable
// state (e.g. chunk bounding boxes computed).
class PLATFORM_EXPORT PaintArtifact final : public RefCounted<PaintArtifact> {
  USING_FAST_MALLOC(PaintArtifact);

 public:
  static scoped_refptr<PaintArtifact> Create(DisplayItemList,
                                             Vector<PaintChunk>);

  static scoped_refptr<PaintArtifact> Empty();

  ~PaintArtifact();

  bool IsEmpty() const { return display_item_list_.IsEmpty(); }

  DisplayItemList& GetDisplayItemList() { return display_item_list_; }
  const DisplayItemList& GetDisplayItemList() const {
    return display_item_list_;
  }

  Vector<PaintChunk>& PaintChunks() { return chunks_; }
  const Vector<PaintChunk>& PaintChunks() const { return chunks_; }

  PaintChunkSubset GetPaintChunkSubset(
      const Vector<wtf_size_t>& subset_indices) const {
    return PaintChunkSubset(PaintChunks(), subset_indices);
  }

  Vector<PaintChunk>::const_iterator FindChunkByDisplayItemIndex(
      size_t index) const {
    return FindChunkInVectorByDisplayItemIndex(PaintChunks(), index);
  }

  // Returns the approximate memory usage, excluding memory likely to be
  // shared with the embedder after copying to cc::DisplayItemList.
  size_t ApproximateUnsharedMemoryUsage() const;

  void AppendDebugDrawing(sk_sp<const PaintRecord>, const PropertyTreeState&);

  // Draws the paint artifact to a GraphicsContext, into the ancestor state
  // given by |replay_state|.
  void Replay(GraphicsContext&,
              const PropertyTreeState& replay_state,
              const IntPoint& offset = IntPoint()) const;

  // Draws the paint artifact to a PaintCanvas, into the ancestor state given
  // by |replay_state|.
  void Replay(cc::PaintCanvas&,
              const PropertyTreeState& replay_state,
              const IntPoint& offset = IntPoint()) const;

  sk_sp<PaintRecord> GetPaintRecord(const PropertyTreeState& replay_state,
                                    const IntPoint& offset = IntPoint()) const;

  // Called when the caller finishes updating a full document life cycle.
  // Will cleanup data (e.g. raster invalidations) that will no longer be used
  // for the next cycle, and update status to be ready for the next cycle.
  void FinishCycle();

 private:
  PaintArtifact();
  PaintArtifact(DisplayItemList, Vector<PaintChunk>);

  DisplayItemList display_item_list_;
  Vector<PaintChunk> chunks_;

  DISALLOW_COPY_AND_ASSIGN(PaintArtifact);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_ARTIFACT_H_
