// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CHUNK_SUBSET_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CHUNK_SUBSET_H_

#include "base/check_op.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// Provides access to a subset of paint chunks in a PaintArtifact.
class PaintChunkSubset {
  DISALLOW_NEW();

 public:
  // An empty subset.
  PaintChunkSubset() = default;

  // A subset containing a single paint chunk initially.
  PaintChunkSubset(scoped_refptr<const PaintArtifact> paint_artifact,
                   wtf_size_t chunk_index)
      : paint_artifact_(std::move(paint_artifact)) {
    DCHECK(paint_artifact_);
    DCHECK(UsesSubsetIndices());
    subset_indices_.push_back(chunk_index);
  }

  // A subset containing the whole PaintArtifact.
  PaintChunkSubset(scoped_refptr<const PaintArtifact> paint_artifact)
      : PaintChunkSubset(paint_artifact,
                         0,
                         paint_artifact->PaintChunks().size()) {}

  // A subset defined by a range of segments. |end_segment_index| is not
  // inclusive.
  PaintChunkSubset(scoped_refptr<const PaintArtifact> paint_artifact,
                   wtf_size_t begin_index,
                   wtf_size_t end_index)
      : paint_artifact_(std::move(paint_artifact)),
        begin_index_(begin_index),
        end_index_(end_index) {
    DCHECK(paint_artifact_);
    DCHECK_LE(begin_index_, end_index_);
    DCHECK(!UsesSubsetIndices());
  }

  class Iterator {
    STACK_ALLOCATED();

   public:
    const PaintChunk& operator*() const { return GetChunk(); }
    const PaintChunk* operator->() const { return &GetChunk(); }
    bool operator==(const Iterator& other) const {
      DCHECK_EQ(subset_, other.subset_);
      return subset_or_chunk_index_ == other.subset_or_chunk_index_;
    }
    bool operator!=(const Iterator& other) const { return !(*this == other); }
    const Iterator& operator++() {
      ++subset_or_chunk_index_;
      return *this;
    }
    const Iterator& operator--() {
      --subset_or_chunk_index_;
      return *this;
    }
    Iterator operator+(wtf_size_t offset) const {
      DCHECK_LE(subset_or_chunk_index_ + offset,
                subset_->end().subset_or_chunk_index_);
      return Iterator(*subset_, subset_or_chunk_index_ + offset);
    }
    Iterator& operator+=(wtf_size_t offset) {
      DCHECK_LE(subset_or_chunk_index_ + offset,
                subset_->end().subset_or_chunk_index_);
      subset_or_chunk_index_ += offset;
      return *this;
    }

    // Returns the index of the current PaintChunk in the PaintArtifact.
    wtf_size_t IndexInPaintArtifact() const {
      if (subset_->UsesSubsetIndices())
        return subset_->subset_indices_[subset_or_chunk_index_];
      return subset_or_chunk_index_;
    }

    DisplayItemRange DisplayItems() const {
      auto& chunk = GetChunk();
      return subset_->paint_artifact_->GetDisplayItemList().ItemsInRange(
          chunk.begin_index, chunk.end_index);
    }

   private:
    friend class PaintChunkSubset;

    Iterator(const PaintChunkSubset& subset, wtf_size_t subset_or_chunk_index)
        : subset_(&subset), subset_or_chunk_index_(subset_or_chunk_index) {}

    const PaintChunk& GetChunk() const {
      DCHECK_LT(subset_or_chunk_index_, subset_->end().subset_or_chunk_index_);
      return subset_->paint_artifact_->PaintChunks()[IndexInPaintArtifact()];
    }

    const PaintChunkSubset* subset_;
    wtf_size_t subset_or_chunk_index_;
  };

  using value_type = PaintChunk;
  using const_iterator = Iterator;

  Iterator begin() const {
    return Iterator(*this, UsesSubsetIndices() ? 0 : begin_index_);
  }

  Iterator end() const {
    return Iterator(*this,
                    UsesSubsetIndices() ? subset_indices_.size() : end_index_);
  }

  bool IsEmpty() const {
    return UsesSubsetIndices() ? subset_indices_.empty()
                               : begin_index_ == end_index_;
  }

  wtf_size_t size() const {
    return UsesSubsetIndices() ? subset_indices_.size()
                               : end_index_ - begin_index_;
  }

  const PaintArtifact& GetPaintArtifact() const { return *paint_artifact_; }

  // This can be used to swap in an updated artifact but care should be taken
  // because the PaintChunk indices into the new artifact must still be valid.
  void SetPaintArtifact(scoped_refptr<const PaintArtifact> paint_artifact) {
    // Existing paint chunk indices would be invalid if the sizes change.
    DCHECK_EQ(paint_artifact->PaintChunks().size(),
              paint_artifact_->PaintChunks().size());
    paint_artifact_ = std::move(paint_artifact);
  }

  void Merge(const PaintChunkSubset& other) {
    DCHECK_EQ(paint_artifact_.get(), other.paint_artifact_.get());
    DCHECK(UsesSubsetIndices());
    DCHECK(other.UsesSubsetIndices());
    subset_indices_.AppendVector(other.subset_indices_);
  }

  size_t ApproximateUnsharedMemoryUsage() const {
    return sizeof(*this) + subset_indices_.CapacityInBytes();
  }

  std::unique_ptr<JSONArray> ToJSON() const;

 private:
  bool UsesSubsetIndices() const { return begin_index_ == kNotFound; }

  scoped_refptr<const PaintArtifact> paint_artifact_;
  // This is used when UsesSubsetIndices() is true.
  Vector<wtf_size_t> subset_indices_;
  // These are used when UsesSubsetIndices() is false.
  wtf_size_t begin_index_ = kNotFound;
  wtf_size_t end_index_ = kNotFound;
};

using PaintChunkIterator = PaintChunkSubset::Iterator;

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&,
                                         const PaintChunkSubset&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CHUNK_SUBSET_H_
