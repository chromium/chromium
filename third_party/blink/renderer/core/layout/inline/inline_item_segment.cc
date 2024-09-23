// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/layout/inline/inline_item_segment.h"

#include "third_party/blink/renderer/core/layout/inline/inline_item.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/run_segmenter.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_buffer.h"

namespace blink {

namespace {

// Constants for PackSegmentData() and UnpackSegmentData().
inline constexpr unsigned kScriptBits = InlineItemSegment::kScriptBits;
inline constexpr unsigned kFontFallbackPriorityBits =
    InlineItemSegment::kFontFallbackPriorityBits;
inline constexpr unsigned kRenderOrientationBits =
    InlineItemSegment::kRenderOrientationBits;

inline constexpr unsigned kScriptMask = (1 << kScriptBits) - 1;
inline constexpr unsigned kFontFallbackPriorityMask =
    (1 << kFontFallbackPriorityBits) - 1;
inline constexpr unsigned kRenderOrientationMask =
    (1 << kRenderOrientationBits) - 1;

static_assert(InlineItemSegment::kSegmentDataBits ==
                  kScriptBits + kRenderOrientationBits +
                      kFontFallbackPriorityBits,
              "kSegmentDataBits must be the sum of these bits");

unsigned SetRenderOrientation(
    unsigned value,
    OrientationIterator::RenderOrientation render_orientation) {
  DCHECK_NE(render_orientation,
            OrientationIterator::RenderOrientation::kOrientationInvalid);
  return (value & ~kRenderOrientationMask) |
         (render_orientation !=
          OrientationIterator::RenderOrientation::kOrientationKeep);
}

}  // namespace

InlineItemSegment::InlineItemSegment(
    const RunSegmenter::RunSegmenterRange& range)
    : end_offset_(range.end), segment_data_(PackSegmentData(range)) {}

InlineItemSegment::InlineItemSegment(unsigned end_offset,
                                     const InlineItem& item)
    : end_offset_(end_offset), segment_data_(item.SegmentData()) {}

unsigned InlineItemSegment::PackSegmentData(
    const RunSegmenter::RunSegmenterRange& range) {
  DCHECK(range.script == USCRIPT_INVALID_CODE ||
         static_cast<unsigned>(range.script) <= kScriptMask);
  DCHECK_LE(static_cast<unsigned>(range.font_fallback_priority),
            kFontFallbackPriorityMask);
  DCHECK_LE(static_cast<unsigned>(range.render_orientation),
            kRenderOrientationMask);

  unsigned value =
      range.script != USCRIPT_INVALID_CODE ? range.script : kScriptMask;
  value <<= kFontFallbackPriorityBits;
  value |= static_cast<unsigned>(range.font_fallback_priority);
  value <<= kRenderOrientationBits;
  value |= range.render_orientation;
  return value;
}

RunSegmenter::RunSegmenterRange InlineItemSegment::UnpackSegmentData(
    unsigned start_offset,
    unsigned end_offset,
    unsigned value) {
  unsigned render_orientation = value & kRenderOrientationMask;
  value >>= kRenderOrientationBits;
  unsigned font_fallback_priority = value & kFontFallbackPriorityMask;
  value >>= kFontFallbackPriorityBits;
  unsigned script = value & kScriptMask;
  return RunSegmenter::RunSegmenterRange{
      start_offset, end_offset,
      script != kScriptMask ? static_cast<UScriptCode>(script)
                            : USCRIPT_INVALID_CODE,
      static_cast<OrientationIterator::RenderOrientation>(render_orientation),
      static_cast<FontFallbackPriority>(font_fallback_priority)};
}

RunSegmenter::RunSegmenterRange InlineItemSegment::ToRunSegmenterRange(
    unsigned start_offset,
    unsigned end_offset) const {
  DCHECK_LT(start_offset, end_offset);
  DCHECK_LT(start_offset, end_offset_);
  return UnpackSegmentData(start_offset, std::min(end_offset, end_offset_),
                           segment_data_);
}

std::unique_ptr<InlineItemSegments> InlineItemSegments::Clone() const {
  auto new_segments = std::make_unique<InlineItemSegments>();
  new_segments->segments_.AppendVector(segments_);
  new_segments->items_to_segments_.AppendVector(items_to_segments_);
  return new_segments;
}

unsigned InlineItemSegments::OffsetForSegment(
    const InlineItemSegment& segment) const {
  return &segment == segments_.data() ? 0 : std::prev(&segment)->EndOffset();
}

#if DCHECK_IS_ON()
void InlineItemSegments::CheckOffset(unsigned offset,
                                     const InlineItemSegment* segment) const {
  DCHECK(segment >= segments_.data() &&
         segment < segments_.data() + segments_.size());
  DCHECK_GE(offset, OffsetForSegment(*segment));
  DCHECK_LT(offset, segment->EndOffset());
}
#endif

void InlineItemSegments::ToRanges(RunSegmenterRanges& ranges) const {
  ranges.ReserveInitialCapacity(segments_.size());
  wtf_size_t start_offset = 0;
  for (const InlineItemSegment& segment : segments_) {
    ranges.push_back(segment.ToRunSegmenterRange(start_offset));
    start_offset = segment.EndOffset();
  }
}

InlineItemSegments::Iterator InlineItemSegments::Ranges(
    unsigned start_offset,
    unsigned end_offset,
    unsigned item_index) const {
  DCHECK_LT(start_offset, end_offset);
  DCHECK_LE(end_offset, EndOffset());

  // Find the first segment for |item_index|.
  unsigned segment_index = items_to_segments_[item_index];
  const InlineItemSegment* segment = &segments_[segment_index];
  DCHECK_GE(start_offset, OffsetForSegment(*segment));
  if (start_offset < segment->EndOffset())
    return Iterator(start_offset, end_offset, segment);

  // The item has multiple segments. Find the segments for |start_offset|.
  unsigned end_segment_index = item_index + 1 < items_to_segments_.size()
                                   ? items_to_segments_[item_index + 1]
                                   : segments_.size();
  CHECK_GT(end_segment_index, segment_index);
  CHECK_LE(end_segment_index, segments_.size());
  segment = std::upper_bound(
      segment, segment + (end_segment_index - segment_index), start_offset,
      [](unsigned offset, const InlineItemSegment& segment) {
        return offset < segment.EndOffset();
      });
  CheckOffset(start_offset, segment);
  return Iterator(start_offset, end_offset, segment);
}

void InlineItemSegments::ComputeSegments(
    RunSegmenter* segmenter,
    RunSegmenter::RunSegmenterRange* range) {
  segments_.Shrink(0);
  do {
    segments_.emplace_back(*range);
  } while (segmenter->Consume(range));
}

unsigned InlineItemSegments::AppendMixedFontOrientation(
    const String& text_content,
    unsigned start_offset,
    unsigned end_offset,
    unsigned segment_index) {
  DCHECK_LT(start_offset, end_offset);
  OrientationIterator iterator(text_content.Characters16() + start_offset,
                               end_offset - start_offset,
                               FontOrientation::kVerticalMixed);
  unsigned original_start_offset = start_offset;
  OrientationIterator::RenderOrientation orientation;
  for (; iterator.Consume(&end_offset, &orientation);
       start_offset = end_offset) {
    end_offset += original_start_offset;
    segment_index = PopulateItemsFromFontOrientation(
        start_offset, end_offset, orientation, segment_index);
  }
  return segment_index;
}

unsigned InlineItemSegments::PopulateItemsFromFontOrientation(
    unsigned start_offset,
    unsigned end_offset,
    OrientationIterator::RenderOrientation render_orientation,
    unsigned segment_index) {
  DCHECK_LT(start_offset, end_offset);
  DCHECK_LE(end_offset, segments_.back().EndOffset());

  while (start_offset >= segments_[segment_index].EndOffset())
    ++segment_index;
  if (start_offset !=
      (segment_index ? segments_[segment_index - 1].EndOffset() : 0u)) {
    Split(segment_index, start_offset);
    ++segment_index;
  }

  for (;; ++segment_index) {
    InlineItemSegment& segment = segments_[segment_index];
    segment.segment_data_ =
        SetRenderOrientation(segment.segment_data_, render_orientation);
    if (end_offset == segment.EndOffset()) {
      ++segment_index;
      break;
    }
    if (end_offset < segment.EndOffset()) {
      Split(segment_index, end_offset);
      ++segment_index;
      break;
    }
  }

  return segment_index;
}

void InlineItemSegments::Split(unsigned index, unsigned offset) {
  InlineItemSegment& segment = segments_[index];
  DCHECK_LT(offset, segment.EndOffset());
  unsigned end_offset = segment.EndOffset();
  segment.end_offset_ = offset;
  segments_.insert(index + 1,
                   InlineItemSegment(end_offset, segment.segment_data_));
}

void InlineItemSegments::ComputeItemIndex(const HeapVector<InlineItem>& items) {
  DCHECK_EQ(items.back().EndOffset(), EndOffset());
  unsigned segment_index = 0;
  const InlineItemSegment* segment = segments_.data();
  unsigned item_index = 0;
  items_to_segments_.resize(items.size());
  for (const InlineItem& item : items) {
    while (segment_index < segments_.size() &&
           item.StartOffset() >= segment->EndOffset()) {
      ++segment_index;
      ++segment;
    }
    items_to_segments_[item_index++] = segment_index;
  }
}

ShapeResult* InlineItemSegments::ShapeText(const HarfBuzzShaper* shaper,
                                           const Font* font,
                                           TextDirection direction,
                                           unsigned start_offset,
                                           unsigned end_offset,
                                           unsigned item_index,
                                           ShapeOptions options) const {
  Vector<RunSegmenter::RunSegmenterRange> ranges;
  for (const RunSegmenter::RunSegmenterRange& range :
       Ranges(start_offset, end_offset, item_index)) {
    ranges.push_back(range);
  }
  ShapeResult* shape_result =
      shaper->Shape(font, direction, start_offset, end_offset, ranges, options);
  DCHECK(shape_result);
  DCHECK_EQ(shape_result->StartIndex(), start_offset);
  DCHECK_EQ(shape_result->EndIndex(), end_offset);
  return shape_result;
}

}  // namespace blink
