/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/geometry/region.h"

#include <stdio.h>

namespace blink {

Region::Region() = default;

Region::Region(const IntRect& rect) : bounds_(rect), shape_(rect) {}

Vector<IntRect> Region::Rects() const {
  Vector<IntRect> rects;

  for (Shape::SpanIterator span = shape_.SpansBegin(), end = shape_.SpansEnd();
       span != end && span + 1 != end; ++span) {
    int y = span->y;
    int height = (span + 1)->y - y;

    for (Shape::SegmentIterator segment = shape_.SegmentsBegin(span),
                                end = shape_.SegmentsEnd(span);
         segment != end && segment + 1 != end; segment += 2) {
      int x = *segment;
      int width = *(segment + 1) - x;

      rects.push_back(IntRect(x, y, width, height));
    }
  }

  return rects;
}

bool Region::Contains(const Region& region) const {
  if (!bounds_.Contains(region.bounds_))
    return false;

  return Shape::CompareShapes<Shape::CompareContainsOperation>(shape_,
                                                               region.shape_);
}

bool Region::Contains(const IntPoint& point) const {
  if (!bounds_.Contains(point))
    return false;

  for (Shape::SpanIterator span = shape_.SpansBegin(), end = shape_.SpansEnd();
       span != end && span + 1 != end; ++span) {
    int y = span->y;
    int max_y = (span + 1)->y;

    if (y > point.Y())
      break;
    if (max_y <= point.Y())
      continue;

    for (Shape::SegmentIterator segment = shape_.SegmentsBegin(span),
                                end = shape_.SegmentsEnd(span);
         segment != end && segment + 1 != end; segment += 2) {
      int x = *segment;
      int max_x = *(segment + 1);

      if (x > point.X())
        break;
      if (max_x > point.X())
        return true;
    }
  }

  return false;
}

bool Region::Intersects(const Region& region) const {
  if (!bounds_.Intersects(region.bounds_))
    return false;

  return Shape::CompareShapes<Shape::CompareIntersectsOperation>(shape_,
                                                                 region.shape_);
}

double Region::Area() const {
  double area = 0.0;
  for (Shape::SpanIterator span = shape_.SpansBegin(), end = shape_.SpansEnd();
       span != end && span + 1 != end; ++span) {
    int height = (span + 1)->y - span->y;

    for (Shape::SegmentIterator segment = shape_.SegmentsBegin(span),
                                end = shape_.SegmentsEnd(span);
         segment != end && segment + 1 != end; segment += 2) {
      int width = *(segment + 1) - *segment;
      area += height * width;
    }
  }
  return area;
}

template <typename CompareOperation>
bool Region::Shape::CompareShapes(const Shape& a_shape, const Shape& b_shape) {
  bool result = CompareOperation::kDefaultResult;

  Shape::SpanIterator a_span = a_shape.SpansBegin();
  Shape::SpanIterator a_span_end = a_shape.SpansEnd();
  Shape::SpanIterator b_span = b_shape.SpansBegin();
  Shape::SpanIterator b_span_end = b_shape.SpansEnd();

  bool a_had_segment_in_previous_span = false;
  bool b_had_segment_in_previous_span = false;
  while (a_span != a_span_end && a_span + 1 != a_span_end &&
         b_span != b_span_end && b_span + 1 != b_span_end) {
    int a_y = a_span->y;
    int a_max_y = (a_span + 1)->y;
    int b_y = b_span->y;
    int b_max_y = (b_span + 1)->y;

    Shape::SegmentIterator a_segment = a_shape.SegmentsBegin(a_span);
    Shape::SegmentIterator a_segment_end = a_shape.SegmentsEnd(a_span);
    Shape::SegmentIterator b_segment = b_shape.SegmentsBegin(b_span);
    Shape::SegmentIterator b_segment_end = b_shape.SegmentsEnd(b_span);

    // Look for a non-overlapping part of the spans. If B had a segment in its
    // previous span, then we already tested A against B within that span.
    bool a_has_segment_in_span = a_segment != a_segment_end;
    bool b_has_segment_in_span = b_segment != b_segment_end;
    if (a_y < b_y && !b_had_segment_in_previous_span && a_has_segment_in_span &&
        CompareOperation::AOutsideB(result))
      return result;
    if (b_y < a_y && !a_had_segment_in_previous_span && b_has_segment_in_span &&
        CompareOperation::BOutsideA(result))
      return result;

    a_had_segment_in_previous_span = a_has_segment_in_span;
    b_had_segment_in_previous_span = b_has_segment_in_span;

    bool spans_overlap = b_max_y > a_y && b_y < a_max_y;
    if (spans_overlap) {
      while (a_segment != a_segment_end && b_segment != b_segment_end) {
        int a_x = *a_segment;
        int a_max_x = *(a_segment + 1);
        int b_x = *b_segment;
        int b_max_x = *(b_segment + 1);

        bool segments_overlap = b_max_x > a_x && b_x < a_max_x;
        if (segments_overlap && CompareOperation::AOverlapsB(result))
          return result;
        if (a_x < b_x && CompareOperation::AOutsideB(result))
          return result;
        if (b_x < a_x && CompareOperation::BOutsideA(result))
          return result;

        if (a_max_x < b_max_x) {
          a_segment += 2;
        } else if (b_max_x < a_max_x) {
          b_segment += 2;
        } else {
          a_segment += 2;
          b_segment += 2;
        }
      }

      if (a_segment != a_segment_end && CompareOperation::AOutsideB(result))
        return result;
      if (b_segment != b_segment_end && CompareOperation::BOutsideA(result))
        return result;
    }

    if (a_max_y < b_max_y) {
      a_span += 1;
    } else if (b_max_y < a_max_y) {
      b_span += 1;
    } else {
      a_span += 1;
      b_span += 1;
    }
  }

  if (a_span != a_span_end && a_span + 1 != a_span_end &&
      CompareOperation::AOutsideB(result))
    return result;
  if (b_span != b_span_end && b_span + 1 != b_span_end &&
      CompareOperation::BOutsideA(result))
    return result;

  return result;
}

void Region::Shape::TrimCapacities() {
  segments_.ShrinkToReasonableCapacity();
  spans_.ShrinkToReasonableCapacity();
}

struct Region::Shape::CompareContainsOperation {
  STATIC_ONLY(CompareContainsOperation);
  const static bool kDefaultResult = true;
  inline static bool AOutsideB(bool& /* result */) { return false; }
  inline static bool BOutsideA(bool& result) {
    result = false;
    return true;
  }
  inline static bool AOverlapsB(bool& /* result */) { return false; }
};

struct Region::Shape::CompareIntersectsOperation {
  STATIC_ONLY(CompareIntersectsOperation);
  const static bool kDefaultResult = false;
  inline static bool AOutsideB(bool& /* result */) { return false; }
  inline static bool BOutsideA(bool& /* result */) { return false; }
  inline static bool AOverlapsB(bool& result) {
    result = true;
    return true;
  }
};

Region::Shape::Shape() = default;

Region::Shape::Shape(const IntRect& rect) {
  AppendSpan(rect.Y());
  AppendSegment(rect.X());
  AppendSegment(rect.MaxX());
  AppendSpan(rect.MaxY());
}

Region::Shape::Shape(wtf_size_t segments_capacity, wtf_size_t spans_capacity) {
  segments_.ReserveCapacity(segments_capacity);
  spans_.ReserveCapacity(spans_capacity);
}

void Region::Shape::AppendSpan(int y) {
  spans_.push_back(Span(y, segments_.size()));
}

bool Region::Shape::CanCoalesce(SegmentIterator begin, SegmentIterator end) {
  if (spans_.IsEmpty())
    return false;

  SegmentIterator last_span_begin =
      segments_.data() + spans_.back().segment_index;
  SegmentIterator last_span_end = segments_.data() + segments_.size();

  // Check if both spans have an equal number of segments.
  if (last_span_end - last_span_begin != end - begin)
    return false;

  // Check if both spans are equal.
  if (!std::equal(begin, end, last_span_begin))
    return false;

  // Since the segments are equal the second segment can just be ignored.
  return true;
}

void Region::Shape::AppendSpan(int y,
                               SegmentIterator begin,
                               SegmentIterator end) {
  if (CanCoalesce(begin, end))
    return;

  AppendSpan(y);
  segments_.AppendRange(begin, end);
}

void Region::Shape::AppendSpans(const Shape& shape,
                                SpanIterator begin,
                                SpanIterator end) {
  for (SpanIterator it = begin; it != end; ++it)
    AppendSpan(it->y, shape.SegmentsBegin(it), shape.SegmentsEnd(it));
}

void Region::Shape::AppendSegment(int x) {
  segments_.push_back(x);
}

Region::Shape::SpanIterator Region::Shape::SpansBegin() const {
  return spans_.data();
}

Region::Shape::SpanIterator Region::Shape::SpansEnd() const {
  return spans_.data() + spans_.size();
}

Region::Shape::SegmentIterator Region::Shape::SegmentsBegin(
    SpanIterator it) const {
  DCHECK_GE(it, spans_.data());
  DCHECK_LT(it, spans_.data() + spans_.size());

  // Check if this span has any segments.
  if (it->segment_index == segments_.size())
    return nullptr;

  return &segments_[it->segment_index];
}

Region::Shape::SegmentIterator Region::Shape::SegmentsEnd(
    SpanIterator it) const {
  DCHECK_GE(it, spans_.data());
  DCHECK_LT(it, spans_.data() + spans_.size());

  // Check if this span has any segments.
  if (it->segment_index == segments_.size())
    return nullptr;

  DCHECK_LT(it + 1, spans_.data() + spans_.size());
  wtf_size_t segment_index = (it + 1)->segment_index;

  SECURITY_DCHECK(segment_index <= segments_.size());
  return segments_.data() + segment_index;
}

#ifndef NDEBUG
void Region::Shape::Dump() const {
  for (Shape::SpanIterator span = SpansBegin(), end = SpansEnd(); span != end;
       ++span) {
    printf("%6d: (", span->y);

    for (Shape::SegmentIterator segment = SegmentsBegin(span),
                                end = SegmentsEnd(span);
         segment != end; ++segment)
      printf("%d ", *segment);
    printf(")\n");
  }

  printf("\n");
}
#endif

IntRect Region::Shape::Bounds() const {
  if (IsEmpty())
    return IntRect();

  SpanIterator span = SpansBegin();
  int min_y = span->y;

  SpanIterator last_span = SpansEnd() - 1;
  int max_y = last_span->y;

  int min_x = std::numeric_limits<int>::max();
  int max_x = std::numeric_limits<int>::min();

  while (span != last_span) {
    SegmentIterator first_segment = SegmentsBegin(span);
    SegmentIterator last_segment = SegmentsEnd(span) - 1;

    if (first_segment && last_segment) {
      DCHECK_NE(first_segment, last_segment);

      if (*first_segment < min_x)
        min_x = *first_segment;

      if (*last_segment > max_x)
        max_x = *last_segment;
    }

    ++span;
  }

  DCHECK_LE(min_x, max_x);
  DCHECK_LE(min_y, max_y);

  return IntRect(min_x, min_y, max_x - min_x, max_y - min_y);
}

void Region::Shape::Translate(const IntSize& offset) {
  for (wtf_size_t i = 0; i < segments_.size(); ++i)
    segments_[i] += offset.Width();
  for (wtf_size_t i = 0; i < spans_.size(); ++i)
    spans_[i].y += offset.Height();
}

void Region::Shape::Swap(Shape& other) {
  segments_.swap(other.segments_);
  spans_.swap(other.spans_);
}

enum {
  kShape1,
  kShape2,
};

template <typename Operation>
Region::Shape Region::Shape::ShapeOperation(const Shape& shape1,
                                            const Shape& shape2) {
  static_assert(!(!Operation::kShouldAddRemainingSegmentsFromSpan1 &&
                  Operation::kShouldAddRemainingSegmentsFromSpan2),
                "invalid segment combination");
  static_assert(!(!Operation::kShouldAddRemainingSpansFromShape1 &&
                  Operation::kShouldAddRemainingSpansFromShape2),
                "invalid span combination");

  wtf_size_t segments_capacity = shape1.SegmentsSize() + shape2.SegmentsSize();
  wtf_size_t spans_capacity = shape1.SpansSize() + shape2.SpansSize();
  Shape result(segments_capacity, spans_capacity);
  if (Operation::TrySimpleOperation(shape1, shape2, result))
    return result;

  SpanIterator spans1 = shape1.SpansBegin();
  SpanIterator spans1_end = shape1.SpansEnd();

  SpanIterator spans2 = shape2.SpansBegin();
  SpanIterator spans2_end = shape2.SpansEnd();

  SegmentIterator segments1 = nullptr;
  SegmentIterator segments1_end = nullptr;

  SegmentIterator segments2 = nullptr;
  SegmentIterator segments2_end = nullptr;

  Vector<int, 32> segments;
  segments.ReserveCapacity(
      std::max(shape1.SegmentsSize(), shape2.SegmentsSize()));

  // Iterate over all spans.
  while (spans1 != spans1_end && spans2 != spans2_end) {
    int y = 0;
    int test = spans1->y - spans2->y;

    if (test <= 0) {
      y = spans1->y;

      segments1 = shape1.SegmentsBegin(spans1);
      segments1_end = shape1.SegmentsEnd(spans1);
      ++spans1;
    }
    if (test >= 0) {
      y = spans2->y;

      segments2 = shape2.SegmentsBegin(spans2);
      segments2_end = shape2.SegmentsEnd(spans2);
      ++spans2;
    }

    int flag = 0;
    int old_flag = 0;

    SegmentIterator s1 = segments1;
    SegmentIterator s2 = segments2;

    // Clear vector without dropping capacity.
    segments.resize(0);
    DCHECK(segments.capacity());

    // Now iterate over the segments in each span and construct a new vector of
    // segments.
    while (s1 != segments1_end && s2 != segments2_end) {
      int test = *s1 - *s2;
      int x;

      if (test <= 0) {
        x = *s1;
        flag = flag ^ 1;
        ++s1;
      }
      if (test >= 0) {
        x = *s2;
        flag = flag ^ 2;
        ++s2;
      }

      if (flag == Operation::kOpCode || old_flag == Operation::kOpCode)
        segments.push_back(x);

      old_flag = flag;
    }

    // Add any remaining segments.
    if (Operation::kShouldAddRemainingSegmentsFromSpan1 && s1 != segments1_end)
      segments.AppendRange(s1, segments1_end);
    else if (Operation::kShouldAddRemainingSegmentsFromSpan2 &&
             s2 != segments2_end)
      segments.AppendRange(s2, segments2_end);

    // Add the span.
    if (!segments.IsEmpty() || !result.IsEmpty())
      result.AppendSpan(y, segments.data(), segments.data() + segments.size());
  }

  // Add any remaining spans.
  if (Operation::kShouldAddRemainingSpansFromShape1 && spans1 != spans1_end)
    result.AppendSpans(shape1, spans1, spans1_end);
  else if (Operation::kShouldAddRemainingSpansFromShape2 &&
           spans2 != spans2_end)
    result.AppendSpans(shape2, spans2, spans2_end);

  result.TrimCapacities();

  return result;
}

struct Region::Shape::UnionOperation {
  STATIC_ONLY(UnionOperation);
  static bool TrySimpleOperation(const Shape& shape1,
                                 const Shape& shape2,
                                 Shape& result) {
    if (shape1.IsEmpty()) {
      result = shape2;
      return true;
    }

    return false;
  }

  static const int kOpCode = 0;

  static const bool kShouldAddRemainingSegmentsFromSpan1 = true;
  static const bool kShouldAddRemainingSegmentsFromSpan2 = true;
  static const bool kShouldAddRemainingSpansFromShape1 = true;
  static const bool kShouldAddRemainingSpansFromShape2 = true;
};

Region::Shape Region::Shape::UnionShapes(const Shape& shape1,
                                         const Shape& shape2) {
  return ShapeOperation<UnionOperation>(shape1, shape2);
}

struct Region::Shape::IntersectOperation {
  STATIC_ONLY(IntersectOperation);
  static bool TrySimpleOperation(const Shape&, const Shape&, Shape&) {
    return false;
  }

  static const int kOpCode = 3;

  static const bool kShouldAddRemainingSegmentsFromSpan1 = false;
  static const bool kShouldAddRemainingSegmentsFromSpan2 = false;
  static const bool kShouldAddRemainingSpansFromShape1 = false;
  static const bool kShouldAddRemainingSpansFromShape2 = false;
};

Region::Shape Region::Shape::IntersectShapes(const Shape& shape1,
                                             const Shape& shape2) {
  return ShapeOperation<IntersectOperation>(shape1, shape2);
}

struct Region::Shape::SubtractOperation {
  STATIC_ONLY(SubtractOperation);
  static bool TrySimpleOperation(const Shape&, const Shape&, Region::Shape&) {
    return false;
  }

  static const int kOpCode = 1;

  static const bool kShouldAddRemainingSegmentsFromSpan1 = true;
  static const bool kShouldAddRemainingSegmentsFromSpan2 = false;
  static const bool kShouldAddRemainingSpansFromShape1 = true;
  static const bool kShouldAddRemainingSpansFromShape2 = false;
};

Region::Shape Region::Shape::SubtractShapes(const Shape& shape1,
                                            const Shape& shape2) {
  return ShapeOperation<SubtractOperation>(shape1, shape2);
}

#ifndef NDEBUG
void Region::Dump() const {
  printf("Bounds: (%d, %d, %d, %d)\n", bounds_.X(), bounds_.Y(),
         bounds_.Width(), bounds_.Height());
  shape_.Dump();
}
#endif

void Region::Intersect(const Region& region) {
  if (bounds_.IsEmpty())
    return;
  if (!bounds_.Intersects(region.bounds_)) {
    shape_ = Shape();
    bounds_ = IntRect();
    return;
  }

  Shape intersected_shape = Shape::IntersectShapes(shape_, region.shape_);

  shape_.Swap(intersected_shape);
  bounds_ = shape_.Bounds();
}

void Region::Unite(const Region& region) {
  if (region.IsEmpty())
    return;
  if (IsRect() && bounds_.Contains(region.bounds_))
    return;
  if (region.IsRect() && region.bounds_.Contains(bounds_)) {
    shape_ = region.shape_;
    bounds_ = region.bounds_;
    return;
  }
  // FIXME: We may want another way to construct a Region without doing this
  // test when we expect it to be false.
  if (!IsRect() && Contains(region))
    return;

  Shape united_shape = Shape::UnionShapes(shape_, region.shape_);

  shape_.Swap(united_shape);
  bounds_.Unite(region.bounds_);
}

void Region::Subtract(const Region& region) {
  if (bounds_.IsEmpty())
    return;
  if (region.IsEmpty())
    return;
  if (!bounds_.Intersects(region.bounds_))
    return;

  Shape subtracted_shape = Shape::SubtractShapes(shape_, region.shape_);

  shape_.Swap(subtracted_shape);
  bounds_ = shape_.Bounds();
}

void Region::Translate(const IntSize& offset) {
  bounds_.Move(offset);
  shape_.Translate(offset);
}

}  // namespace blink
