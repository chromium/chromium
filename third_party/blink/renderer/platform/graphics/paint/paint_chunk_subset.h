// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CHUNK_SUBSET_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CHUNK_SUBSET_H_

#include <iosfwd>

#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// Provides access to a subset of paint chunks in a PaintArtifact.
// TODO(wangxianzhu): When we remove pre-CAP, we can split this class for the
// two use cases (corresponding to the two constructors respectively): One for
// iterating paint chunks in a paint artifact within a segment range, and the
// other for PaintArtifactCompositor PendingLayer to create a subset of paint
// chunks from indices. For now we still need this class to provide a common API
// for the two use cases for RasterInvalidator and PaintChunksToCcLayer, etc.
class PaintChunkSubset {
  STACK_ALLOCATED();

 public:
  // A subset defined by a vector of paint chunk indices. It references
  // |subset_indices| provided by the caller, so it should not live longer than
  // |subset_indices|.
  PaintChunkSubset(scoped_refptr<const PaintArtifact> paint_artifact,
                   const Vector<PaintChunkIndex>& subset_indices)
      : paint_artifact_(std::move(paint_artifact)),
        subset_indices_(&subset_indices) {}

  // A subset defined by a range of segments. |end_segment_index| is not
  // inclusive.
  PaintChunkSubset(scoped_refptr<const PaintArtifact> paint_artifact,
                   wtf_size_t begin_segment_index,
                   wtf_size_t end_segment_index)
      : paint_artifact_(std::move(paint_artifact)),
        begin_segment_index_(begin_segment_index),
        end_segment_index_(end_segment_index) {
    DCHECK_LE(begin_segment_index_, end_segment_index_);
  }

  class Iterator {
    STACK_ALLOCATED();

   public:
    const PaintChunk& operator*() const { return subset_[Index()]; }
    const PaintChunk* operator->() const { return &subset_[Index()]; }
    bool operator==(const Iterator& other) const {
      DCHECK_EQ(&subset_, &other.subset_);
      if (subset_.subset_indices_)
        return subset_index_ == other.subset_index_;
      return chunk_index_ == other.chunk_index_;
    }
    bool operator!=(const Iterator& other) const { return !(*this == other); }
    const Iterator& operator++() {
      if (subset_.subset_indices_)
        subset_index_++;
      else
        subset_.paint_artifact_->IncrementChunkIndex(chunk_index_);
      return *this;
    }

    // Returns the index of the current PaintChunk in the PaintArtifact.
    PaintChunkIndex Index() const {
      if (subset_.subset_indices_)
        return (*subset_.subset_indices_)[subset_index_];
      return chunk_index_;
    }

    DisplayItemRange DisplayItems() const {
      return subset_.paint_artifact_->DisplayItemsInChunk(Index());
    }

    String ToString() const { return Index().ToString(); }

   private:
    friend class PaintChunkSubset;

    Iterator(const PaintChunkSubset& subset, PaintChunkIndex chunk_index)
        : subset_(subset), chunk_index_(chunk_index) {}
    Iterator(const PaintChunkSubset& subset, wtf_size_t subset_index)
        : subset_(subset), subset_index_(subset_index) {}

    const PaintChunkSubset& subset_;
    union {
      PaintChunkIndex chunk_index_;  // If subset_.subset_indices_ is nullptr.
      wtf_size_t subset_index_;  // If subset_.subset_indices_ is not nullptr.
    };
  };

  using value_type = PaintChunk;
  using const_iterator = Iterator;

  Iterator begin() const {
    if (subset_indices_)
      return Iterator(*this, 0);
    return Iterator(*this, {begin_segment_index_, 0});
  }

  Iterator end() const {
    if (subset_indices_)
      return Iterator(*this, subset_indices_->size());
    return Iterator(*this, {end_segment_index_, 0});
  }

  bool IsEmpty() const {
    if (subset_indices_)
      return subset_indices_->IsEmpty();
    return begin_segment_index_ == end_segment_index_;
  }

  const PaintChunk& operator[](PaintChunkIndex i) const {
    return paint_artifact_->GetChunk(i);
  }

  const PaintArtifact& GetPaintArtifact() const { return *paint_artifact_; }

 private:
  scoped_refptr<const PaintArtifact> paint_artifact_;
  const Vector<PaintChunkIndex>* subset_indices_ = nullptr;
  // These are used when subset_indices is nullptr.
  wtf_size_t begin_segment_index_ = kNotFound;
  wtf_size_t end_segment_index_ = kNotFound;
};

using PaintChunkIterator = PaintChunkSubset::Iterator;

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, PaintChunkIterator);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CHUNK_SUBSET_H_
