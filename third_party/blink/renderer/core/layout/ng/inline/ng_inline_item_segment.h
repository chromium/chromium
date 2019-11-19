// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEM_SEGMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEM_SEGMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/core/layout/ng/ng_style_variant.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/shaping/run_segmenter.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

#include <unicode/ubidi.h>
#include <unicode/uscript.h>

namespace blink {

class HarfBuzzShaper;
class NGInlineItem;

// Represents a segment produced by |RunSegmenter|.
//
// |RunSegmenter| is forward-only that this class provides random access to the
// result by keeping in memory. This class packs the data in a compact form to
// minimize the memory impact.
class CORE_EXPORT NGInlineItemSegment {
  DISALLOW_NEW();

 public:
  NGInlineItemSegment(unsigned end_offset, unsigned segment_data)
      : end_offset_(end_offset), segment_data_(segment_data) {}
  NGInlineItemSegment(const RunSegmenter::RunSegmenterRange& range);
  NGInlineItemSegment(unsigned end_offset, const NGInlineItem& item);

  RunSegmenter::RunSegmenterRange ToRunSegmenterRange(
      unsigned start_offset,
      unsigned end_offset) const;

  unsigned EndOffset() const { return end_offset_; }

  // Pack/unpack utility functions to store in bit fields.
  static constexpr unsigned kSegmentDataBits = 11;

  static unsigned PackSegmentData(const RunSegmenter::RunSegmenterRange& range);
  static RunSegmenter::RunSegmenterRange
  UnpackSegmentData(unsigned start_offset, unsigned end_offset, unsigned value);

 private:
  unsigned end_offset_;
  unsigned segment_data_ : kSegmentDataBits;

  friend class NGInlineItemSegments;
};

// Represents a set of |NGInlineItemSegment| for an inline formatting context
// represented by |NGInlineItemsData|.
//
// The segments/block ratio for Latin is 1.0 to 1.01 in average, while it
// increases to 1.01 to 1.05 for most other writing systems because it is common
// to have some Latin words within paragraphs.
//
// For writing systems that has multiple native scripts such as Japanese, the
// ratio jumps to 10-30, or sometimes 300 depends on the length of the block,
// because the average characters/segment ratio in Japanese is 2-5. This class
// builds internal indexes for faster access in such cases.
class CORE_EXPORT NGInlineItemSegments {
  USING_FAST_MALLOC(NGInlineItemSegments);

 public:
  unsigned size() const { return segments_.size(); }
  bool IsEmpty() const { return segments_.IsEmpty(); }

  // Start/end offset of each segment/entire segments.
  unsigned OffsetForSegment(const NGInlineItemSegment& segment) const;
  unsigned EndOffset() const { return segments_.back().EndOffset(); }

  void ReserveCapacity(unsigned capacity) {
    segments_.ReserveCapacity(capacity);
  }

  // Append a |NGInlineItemSegment| using one of its constructors.
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
  unsigned AppendMixedFontOrientation(const String& text_content,
                                      unsigned start_offset,
                                      unsigned end_offset,
                                      unsigned segment_index);

  // Compute an internal items-to-segments index for faster access.
  void ComputeItemIndex(const Vector<NGInlineItem>& items);

  // Iterates |RunSegmenterRange| for the given offsets.
  class Iterator {
    STACK_ALLOCATED();

   public:
    Iterator(unsigned start_offset,
             unsigned end_offset,
             const NGInlineItemSegment* segment);

    bool IsDone() const { return range_.start == end_offset_; }

    // This class provides both iterator and range for the simplicity.
    const Iterator& begin() const { return *this; }
    const Iterator& end() const { return *this; }

    bool operator!=(const Iterator& other) const { return !IsDone(); }
    const RunSegmenter::RunSegmenterRange& operator*() const { return range_; }
    void operator++();

   private:
    RunSegmenter::RunSegmenterRange range_;
    const NGInlineItemSegment* segment_;
    unsigned start_offset_;
    unsigned end_offset_;
  };
  using const_iterator = Iterator;

  // Returns an iterator for the given offsets.
  //
  // |item_index| is the index of |NGInlineItem| for the |start_offset|.
  const_iterator Ranges(unsigned start_offset,
                        unsigned end_offset,
                        unsigned item_index) const;

  // Shape runs in the range and return the concatenated |ShapeResult|.
  scoped_refptr<ShapeResult> ShapeText(const HarfBuzzShaper* shaper,
                                       const Font* font,
                                       TextDirection direction,
                                       unsigned start_offset,
                                       unsigned end_offset,
                                       unsigned item_index) const;

 private:
  unsigned PopulateItemsFromFontOrientation(
      unsigned start_offset,
      unsigned end_offset,
      OrientationIterator::RenderOrientation,
      unsigned segment_index);
  void Split(unsigned index, unsigned offset);

#if DCHECK_IS_ON()
  void CheckOffset(unsigned offset, const NGInlineItemSegment* segment) const;
#else
  void CheckOffset(unsigned offset, const NGInlineItemSegment* segment) const {}
#endif

  Vector<NGInlineItemSegment> segments_;
  Vector<unsigned> items_to_segments_;
};

inline NGInlineItemSegments::Iterator::Iterator(
    unsigned start_offset,
    unsigned end_offset,
    const NGInlineItemSegment* segment)
    : segment_(segment), start_offset_(start_offset), end_offset_(end_offset) {
  DCHECK_LT(start_offset, end_offset);
  DCHECK_LT(start_offset, segment->EndOffset());
  range_ = segment->ToRunSegmenterRange(start_offset_, end_offset_);
}

inline void NGInlineItemSegments::Iterator::operator++() {
  DCHECK_LE(range_.end, end_offset_);
  if (range_.end == end_offset_) {
    range_.start = end_offset_;
    return;
  }
  start_offset_ = range_.end;
  ++segment_;
  range_ = segment_->ToRunSegmenterRange(start_offset_, end_offset_);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEM_SEGMENT_H_
