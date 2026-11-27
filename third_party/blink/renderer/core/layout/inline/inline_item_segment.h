// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_INLINE_ITEM_SEGMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_INLINE_ITEM_SEGMENT_H_

#include <unicode/ubidi.h>
#include <unicode/uscript.h>

#include <bit>
#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/inline/offset_mapping.h"
#include "third_party/blink/renderer/core/layout/style_variant.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/shaping/run_segmenter.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_options.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"

namespace blink {

class HarfBuzzShaper;
class InlineItem;
class TextOffsetMap;

// Represents a segment produced by |RunSegmenter|.
//
// |RunSegmenter| is forward-only that this class provides random access to the
// result by keeping in memory. This class packs the data in a compact form to
// minimize the memory impact.
class CORE_EXPORT InlineItemSegment {
  DISALLOW_NEW();

 public:
  InlineItemSegment(wtf_size_t end_offset, uint32_t segment_data)
      : end_offset_(end_offset), segment_data_(segment_data) {}
  InlineItemSegment(const RunSegmenter::RunSegmenterRange& range);
  InlineItemSegment(wtf_size_t end_offset, const InlineItem& item);

  RunSegmenter::RunSegmenterRange ToRunSegmenterRange(
      wtf_size_t start_offset,
      wtf_size_t end_offset) const;

  wtf_size_t EndOffset() const { return end_offset_; }

  // Pack/unpack utility functions to store in bit fields.
  // UScriptCode is -1 (USCRIPT_INVALID_CODE) to 199 as of ICU 74.
  // `USCRIPT_CODE_LIMIT` is the max value plus 1, but the plus 1 is necessary
  // to represent -1 (USCRIPT_INVALID_CODE), which is handled separately.
  static constexpr wtf_size_t kScriptBits =
      std::bit_width(static_cast<uint32_t>(USCRIPT_CODE_LIMIT));
  static constexpr wtf_size_t kFontFallbackPriorityBits =
      std::bit_width(std::to_underlying(FontFallbackPriority::kMaxEnumValue));
  static constexpr wtf_size_t kRenderOrientationBits =
      std::bit_width(std::to_underlying(
          OrientationIterator::RenderOrientation::kMaxEnumValue));
  static constexpr wtf_size_t kSegmentDataBits =
      kScriptBits + kFontFallbackPriorityBits + kRenderOrientationBits;

  static uint32_t PackSegmentData(const RunSegmenter::RunSegmenterRange& range);
  static RunSegmenter::RunSegmenterRange UnpackSegmentData(
      wtf_size_t start_offset,
      wtf_size_t end_offset,
      uint32_t value);

 private:
  wtf_size_t end_offset_;
  uint32_t segment_data_ : kSegmentDataBits;

  friend class InlineItemSegments;
};

// Represents a set of |InlineItemSegment| for an inline formatting context
// represented by |InlineItemsData|.
//
// The segments/block ratio for Latin is 1.0 to 1.01 in average, while it
// increases to 1.01 to 1.05 for most other writing systems because it is common
// to have some Latin words within paragraphs.
//
// For writing systems that has multiple native scripts such as Japanese, the
// ratio jumps to 10-30, or sometimes 300 depends on the length of the block,
// because the average characters/segment ratio in Japanese is 2-5. This class
// builds internal indexes for faster access in such cases.
class CORE_EXPORT InlineItemSegments
    : public GarbageCollected<InlineItemSegments> {
 public:
  InlineItemSegments* Clone() const;

  wtf_size_t size() const { return segments_.size(); }
  bool IsEmpty() const { return segments_.empty(); }

  // Start/end offset of each segment/entire segments.
  wtf_size_t OffsetForSegment(const InlineItemSegment& segment) const;
  wtf_size_t EndOffset() const { return segments_.back().EndOffset(); }

  void ReserveCapacity(wtf_size_t capacity) { segments_.reserve(capacity); }

  // Append a |InlineItemSegment| using one of its constructors.
  template <class... Args>
  void Append(Args&&... args) {
    segments_.emplace_back(std::forward<Args>(args)...);
  }

  // Compute segments from the given |RunSegmenter|.
  void ComputeSegments(RunSegmenter* segmenter,
                       RunSegmenter::RunSegmenterRange* range);

  // Append mixed-vertical font orientation segments for the specified range.
  // This is separated from |ComputeSegments| because this result depends on
  // fonts.
  wtf_size_t AppendMixedFontOrientation(const String& text_content,
                                        wtf_size_t start_offset,
                                        wtf_size_t end_offset,
                                        wtf_size_t segment_index);

  // Adjust offsets according to the `offset_map`.
  void AdjustOffsets(const TextOffsetMap& offset_map);

  // Compute an internal items-to-segments index for faster access.
  void ComputeItemIndex(const HeapVector<Member<InlineItem>>& items);

  // Iterates |RunSegmenterRange| for the given offsets.
  class Iterator {
    STACK_ALLOCATED();

   public:
    Iterator(wtf_size_t start_offset,
             wtf_size_t end_offset,
             base::span<const InlineItemSegment> span,
             wtf_size_t segment_index);

    bool IsDone() const { return range_.start == end_offset_; }

    // This class provides both iterator and range for the simplicity.
    const Iterator& begin() const { return *this; }
    const Iterator& end() const { return *this; }

    bool operator!=(const Iterator& other) const { return !IsDone(); }
    const RunSegmenter::RunSegmenterRange& operator*() const { return range_; }
    void operator++();

   private:
    RunSegmenter::RunSegmenterRange range_;
    base::span<const InlineItemSegment> span_;
    wtf_size_t segment_index_;
    wtf_size_t start_offset_;
    wtf_size_t end_offset_;
  };
  using const_iterator = Iterator;

  // Returns an iterator for the given offsets.
  //
  // |item_index| is the index of |InlineItem| for the |start_offset|.
  const_iterator Ranges(wtf_size_t start_offset,
                        wtf_size_t end_offset,
                        wtf_size_t item_index) const;

  // Shape runs in the range and return the concatenated |ShapeResult|.
  ShapeResult* ShapeText(const HarfBuzzShaper* shaper,
                         const Font* font,
                         TextDirection direction,
                         wtf_size_t start_offset,
                         wtf_size_t end_offset,
                         wtf_size_t item_index,
                         ShapeOptions = ShapeOptions()) const;

  void Trace(Visitor* visitor) const {
    visitor->Trace(segments_);
    visitor->Trace(items_to_segments_);
  }

 private:
  wtf_size_t PopulateItemsFromFontOrientation(
      wtf_size_t start_offset,
      wtf_size_t end_offset,
      OrientationIterator::RenderOrientation,
      wtf_size_t segment_index);
  void Split(wtf_size_t index, wtf_size_t offset);

#if DCHECK_IS_ON()
  void CheckOffset(wtf_size_t offset, const InlineItemSegment* segment) const;
#else
  void CheckOffset(wtf_size_t offset, const InlineItemSegment* segment) const {}
#endif

  HeapVector<InlineItemSegment> segments_;
  HeapVector<wtf_size_t> items_to_segments_;
};

inline InlineItemSegments::Iterator::Iterator(
    wtf_size_t start_offset,
    wtf_size_t end_offset,
    base::span<const InlineItemSegment> span,
    wtf_size_t segment_index)
    : span_(span),
      segment_index_(segment_index),
      start_offset_(start_offset),
      end_offset_(end_offset) {
  DCHECK_LT(start_offset, end_offset);
  DCHECK_LT(start_offset, span[segment_index].EndOffset());
  range_ = span[segment_index].ToRunSegmenterRange(start_offset_, end_offset_);
}

inline void InlineItemSegments::Iterator::operator++() {
  DCHECK_LE(range_.end, end_offset_);
  if (range_.end == end_offset_) {
    range_.start = end_offset_;
    return;
  }
  start_offset_ = range_.end;
  ++segment_index_;
  range_ =
      span_[segment_index_].ToRunSegmenterRange(start_offset_, end_offset_);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_INLINE_ITEM_SEGMENT_H_
