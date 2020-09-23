// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_ARTIFACT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_ARTIFACT_H_

#include <iosfwd>

#include "third_party/blink/renderer/platform/graphics/paint/display_item_list.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {
class PaintCanvas;
}

namespace blink {

class GraphicsContext;
class PaintChunkSubset;

// Contains the indices to locate a paint chunk in a paint artifact. See the
// comments for PaintArtifact below for the reason why we need two indices.
// Most users of this struct should treat it as an opaque data structure.
struct PLATFORM_EXPORT PaintChunkIndex {
  wtf_size_t segment_index = 0;
  wtf_size_t chunk_index = 0;

  bool operator==(PaintChunkIndex other) const {
    return segment_index == other.segment_index &&
           chunk_index == other.chunk_index;
  }
  bool operator!=(PaintChunkIndex other) const { return !(*this == other); }

  String ToString() const;
};

// Similar to PaintChunkIndex, but for display items.
struct PLATFORM_EXPORT DisplayItemIndex {
  wtf_size_t segment_index = 0;
  wtf_size_t item_index = 0;

  bool operator==(DisplayItemIndex other) const {
    return segment_index == other.segment_index &&
           item_index == other.item_index;
  }
  bool operator!=(DisplayItemIndex other) const { return !(*this == other); }

  String ToString() const;
};

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, PaintChunkIndex);
PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, DisplayItemIndex);

// A PaintArtifact represents the output of painting, consisting of paint chunks
// and display items which are stored in a two-level vector:
// - The first level vector contains the second level vectors which are
//   called "segments". Segments are the unit of subsequence caching.
// - For paint chunks, the second level vectors are Vector<PaintChunk>.
// - For display items, the second level vectors are DisplayItemList.
//
// A PaintArtifact not only represent the output of the current painting, but
// also serve as cache of individual display items and paint chunks for later
// paintings as long as the display items and paint chunks are valid.
//
// It represents a particular state of the world, and is immutable (const) and
// promises to be in a reasonable state (e.g. chunk bounding boxes computed) to
// all users, except for PaintController and unit tests.
//
// Empty segments are not allowed except the following cases:
// TODO(wangxianzhu): the cases don't exist yet.
//
// A segment containing empty |display_item_list| is valid because we can have
// paint chunks without any display items, e.g. for hit testing.
//
class PLATFORM_EXPORT PaintArtifact final : public RefCounted<PaintArtifact> {
  USING_FAST_MALLOC(PaintArtifact);

 public:
  PaintArtifact();
  ~PaintArtifact();

  PaintArtifact(const PaintArtifact& other) = delete;
  PaintArtifact& operator=(const PaintArtifact& other) = delete;
  PaintArtifact(PaintArtifact&& other) = delete;
  PaintArtifact& operator=(PaintArtifact&& other) = delete;

  // See class comment for details about emptiness.
  bool IsEmpty() const { return segments_.IsEmpty(); }

  void AddSegment(Vector<PaintChunk> chunks, DisplayItemList list) {
    // We don't allow empty segments.
    DCHECK(!chunks.IsEmpty());
    // |list| can be empty.
    segments_.push_back(Segment{std::move(chunks), std::move(list)});
  }

  const PaintChunk& GetChunk(PaintChunkIndex i) const {
    CheckChunkIndex(i);
    return segments_[i.segment_index].chunks[i.chunk_index];
  }

  void IncrementChunkIndex(PaintChunkIndex& i) const {
    CheckChunkIndex(i);
    DCHECK(!segments_[i.segment_index].chunks.IsEmpty());
    if (i.chunk_index < segments_[i.segment_index].chunks.size() - 1) {
      i.chunk_index++;
      return;
    }
    i.chunk_index = 0;
    i.segment_index++;
  }

  const DisplayItem& GetDisplayItem(DisplayItemIndex i) const {
    CheckDisplayItemIndex(i);
    return segments_[i.segment_index].display_item_list[i.item_index];
  }

  void IncrementDisplayItemIndex(DisplayItemIndex& i) const {
    CheckDisplayItemIndex(i);
    DCHECK(!segments_[i.segment_index].display_item_list.IsEmpty());
    if (i.item_index <
        segments_[i.segment_index].display_item_list.size() - 1) {
      i.item_index++;
      return;
    }
    i.item_index = 0;
    do {
      i.segment_index++;
    } while (i.segment_index < segments_.size() &&
             segments_[i.segment_index].display_item_list.IsEmpty());
  }

  DisplayItemRange DisplayItemsInChunk(PaintChunkIndex i) const {
    CheckChunkIndex(i);
    auto& chunk = GetChunk(i);
    return segments_[i.segment_index].display_item_list.ItemsInRange(
        chunk.begin_index, chunk.end_index);
  }

  // The returned PaintChunkSubset holds a reference to this PaintArtifact.
  // It can be used to iterate all paint chunks (and display items through
  // paint chunk iterators) in this PaintArtifact.
  PaintChunkSubset Chunks() const;

  // TODO(wangxianzhu): These temporarily adapt for the existing callers.
  // Remove when callers support multiple segments.
  DisplayItemList& GetDisplayItemList() {
    DCHECK_EQ(1u, segments_.size());
    return segments_[0].display_item_list;
  }
  const DisplayItemList& GetDisplayItemList() const {
    DCHECK_EQ(1u, segments_.size());
    return segments_[0].display_item_list;
  }
  Vector<PaintChunk>& DeprecatedChunks() {
    DCHECK_EQ(1u, segments_.size());
    return segments_[0].chunks;
  }
  const Vector<PaintChunk>& DeprecatedChunks() const {
    DCHECK_EQ(1u, segments_.size());
    return segments_[0].chunks;
  }

  // Returns the approximate memory usage, excluding memory likely to be
  // shared with the embedder after copying to cc::DisplayItemList.
  size_t ApproximateUnsharedMemoryUsage() const;

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
  void CheckChunkIndex(PaintChunkIndex i) const {
    DCHECK_LT(i.segment_index, segments_.size());
    DCHECK_LT(i.chunk_index, segments_[i.segment_index].chunks.size());
  }

  void CheckDisplayItemIndex(DisplayItemIndex i) const {
    DCHECK_LT(i.segment_index, segments_.size());
    DCHECK_LT(i.item_index,
              segments_[i.segment_index].display_item_list.size());
  }

  struct Segment {
    Vector<PaintChunk> chunks;
    DisplayItemList display_item_list;
  };
  Vector<Segment> segments_;
};

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, PaintChunkIndex);
PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, DisplayItemIndex);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_ARTIFACT_H_
