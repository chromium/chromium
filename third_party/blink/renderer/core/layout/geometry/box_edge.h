// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_BOX_EDGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_BOX_EDGE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

// BoxEdge is a one-dimensional (1D) position and size (which
// typically represents the horizontal or vertical projection of a rect).
struct CORE_EXPORT BoxEdge {
  LayoutUnit offset;
  LayoutUnit size;

  constexpr BoxEdge() = default;
  constexpr BoxEdge(LayoutUnit offset, LayoutUnit size)
      : offset(offset), size(size) {}

  constexpr LayoutUnit End() const { return offset + size; }
  constexpr bool IsEmpty() const { return size <= LayoutUnit(); }

  // Geometric functionality
  constexpr bool Contains(LayoutUnit value) const {
    return value >= offset && value < End();
  }
  constexpr bool Contains(BoxEdge other) const {
    return offset <= other.offset && End() >= other.End();
  }
  constexpr bool Intersects(BoxEdge other) const {
    return !IsEmpty() && !other.IsEmpty() && End() > other.offset &&
           offset < other.End();
  }

  // Shifts the entire segment by a given offset.
  constexpr void Move(LayoutUnit delta) { offset += delta; }

  constexpr BoxEdge& operator+=(LayoutUnit delta) {
    offset += delta;
    return *this;
  }
  constexpr BoxEdge& operator-=(LayoutUnit delta) {
    offset -= delta;
    return *this;
  }

  constexpr bool operator==(const BoxEdge& other) const = default;
};

inline constexpr BoxEdge operator+(BoxEdge segment, LayoutUnit delta) {
  return BoxEdge(segment.offset + delta, segment.size);
}

inline constexpr BoxEdge operator-(BoxEdge segment, LayoutUnit delta) {
  return BoxEdge(segment.offset - delta, segment.size);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_BOX_EDGE_H_
