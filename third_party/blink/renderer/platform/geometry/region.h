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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_REGION_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_REGION_H_

#include "cc/base/region.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace cc {
class Region;
}

namespace blink {

// A region class based on the paper "Scanline Coherent Shape Algebra"
// by Jonathan E. Steinhart from the book "Graphics Gems II".
//
// This implementation uses two vectors instead of linked list, and
// also compresses regions when possible.

class PLATFORM_EXPORT Region {
  DISALLOW_NEW();

 public:
  Region();
  Region(const IntRect&);

  IntRect Bounds() const { return bounds_; }
  bool IsEmpty() const { return bounds_.IsEmpty(); }
  bool IsRect() const { return shape_.IsRect(); }

  Vector<IntRect> Rects() const;

  void Unite(const Region&);
  void Intersect(const Region&);
  void Subtract(const Region&);

  void Translate(const IntSize&);

  // Returns true if the query region is a subset of this region.
  bool Contains(const Region&) const;

  bool Contains(const IntPoint&) const;

  // Returns true if the query region intersects any part of this region.
  bool Intersects(const Region&) const;

  uint64_t Area() const;

#ifndef NDEBUG
  void Dump() const;
#endif

 private:
  struct Span {
    DISALLOW_NEW();
    Span(int y, wtf_size_t segment_index)
        : y(y), segment_index(segment_index) {}

    int y;
    wtf_size_t segment_index;
  };

  // Shape composed of non-overlapping rectangles implied by segments [x, max_x)
  // within spans [y, max_y).
  //
  // Segment iteration returns x and max_x for each segment in a span, in order.
  // Span iteration returns y via the Span object; spans are adjacent, so max_y
  // is the next Span's y value. (The last Span contains no segments.)

  class Shape {
    DISALLOW_NEW();

   public:
    Shape();
    Shape(const IntRect&);
    Shape(wtf_size_t segments_capacity, wtf_size_t spans_capacity);

    IntRect Bounds() const;
    bool IsEmpty() const { return spans_.IsEmpty(); }
    bool IsRect() const { return spans_.size() <= 2 && segments_.size() <= 2; }

    typedef const Span* SpanIterator;
    SpanIterator SpansBegin() const;
    SpanIterator SpansEnd() const;
    wtf_size_t SpansSize() const { return spans_.size(); }

    typedef const int* SegmentIterator;
    SegmentIterator SegmentsBegin(SpanIterator) const;
    SegmentIterator SegmentsEnd(SpanIterator) const;
    wtf_size_t SegmentsSize() const { return segments_.size(); }

    static Shape UnionShapes(const Shape& shape1, const Shape& shape2);
    static Shape IntersectShapes(const Shape& shape1, const Shape& shape2);
    static Shape SubtractShapes(const Shape& shape1, const Shape& shape2);

    void Translate(const IntSize&);
    void Swap(Shape&);

    struct CompareContainsOperation;
    struct CompareIntersectsOperation;

    template <typename CompareOperation>
    static bool CompareShapes(const Shape& shape1, const Shape& shape2);
    void TrimCapacities();

#ifndef NDEBUG
    void Dump() const;
#endif

   private:
    struct UnionOperation;
    struct IntersectOperation;
    struct SubtractOperation;

    template <typename Operation>
    static Shape ShapeOperation(const Shape& shape1, const Shape& shape2);

    void AppendSegment(int x);
    void AppendSpan(int y);
    void AppendSpan(int y, SegmentIterator begin, SegmentIterator end);
    void AppendSpans(const Shape&, SpanIterator begin, SpanIterator end);

    bool CanCoalesce(SegmentIterator begin, SegmentIterator end);

    // Stores all segments for all spans, in order.  Each Span's segment_index
    // identifies the start of its segments within this vector.
    Vector<int, 32> segments_;

    Vector<Span, 16> spans_;

    friend bool operator==(const Shape&, const Shape&);
  };

  IntRect bounds_;
  Shape shape_;

  friend bool operator==(const Region&, const Region&);
  friend bool operator==(const Shape&, const Shape&);
  friend bool operator==(const Span&, const Span&);
};

static inline Region Intersect(const Region& a, const Region& b) {
  Region result(a);
  result.Intersect(b);

  return result;
}

static inline Region Subtract(const Region& a, const Region& b) {
  Region result(a);
  result.Subtract(b);

  return result;
}

static inline Region Translate(const Region& region, const IntSize& offset) {
  Region result(region);
  result.Translate(offset);

  return result;
}

// Creates a cc::Region with the same data as |region|.
static inline cc::Region RegionToCCRegion(const Region& in_region) {
  Vector<IntRect> rects = in_region.Rects();
  cc::Region out_region;
  for (const IntRect& r : rects)
    out_region.Union(gfx::Rect(r.X(), r.Y(), r.Width(), r.Height()));
  return out_region;
}

inline bool operator==(const Region& a, const Region& b) {
  return a.bounds_ == b.bounds_ && a.shape_ == b.shape_;
}

inline bool operator==(const Region::Shape& a, const Region::Shape& b) {
  return a.spans_ == b.spans_ && a.segments_ == b.segments_;
}

inline bool operator==(const Region::Span& a, const Region::Span& b) {
  return a.y == b.y && a.segment_index == b.segment_index;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_REGION_H_
