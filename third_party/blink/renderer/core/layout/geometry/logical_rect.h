// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_LOGICAL_RECT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_LOGICAL_RECT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"

namespace blink {

class LayoutRect;

// LogicalRect is the position and size of a rect (typically a fragment)
// relative to the parent in the logical coordinate system.
// For more information about physical and logical coordinate systems, see:
// https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/core/layout/README.md#coordinate-spaces
struct CORE_EXPORT LogicalRect {
  constexpr LogicalRect() = default;
  constexpr LogicalRect(const LogicalOffset& offset, const LogicalSize& size)
      : offset(offset), size(size) {}
  constexpr LogicalRect(LayoutUnit inline_offset,
                        LayoutUnit block_offset,
                        LayoutUnit inline_size,
                        LayoutUnit block_size)
      : offset(inline_offset, block_offset), size(inline_size, block_size) {}

  // This is deleted to avoid unwanted lossy conversion from float or double to
  // LayoutUnit or int. Use explicit LayoutUnit constructor for each parameter
  // instead.
  LogicalRect(double, double, double, double) = delete;

  // For testing only. It's defined in core/testing/core_unit_test_helper.h.
  // 'constexpr' is to let compiler detect usage from production code.
  constexpr LogicalRect(int inline_offset,
                        int block_offset,
                        int inline_size,
                        int block_size);

  constexpr explicit LogicalRect(const LayoutRect& source)
      : LogicalRect({source.X(), source.Y()},
                    {source.Width(), source.Height()}) {}

  constexpr LayoutRect ToLayoutRect() const {
    return {offset.inline_offset, offset.block_offset, size.inline_size,
            size.block_size};
  }

  LogicalOffset offset;
  LogicalSize size;

  constexpr bool IsEmpty() const { return size.IsEmpty(); }

  LayoutUnit InlineEndOffset() const {
    return offset.inline_offset + size.inline_size;
  }
  LayoutUnit BlockEndOffset() const {
    return offset.block_offset + size.block_size;
  }
  LogicalOffset EndOffset() const { return offset + size; }

  constexpr bool operator==(const LogicalRect& other) const {
    return other.offset == offset && other.size == size;
  }

  LogicalRect operator+(const LogicalOffset& additional_offset) const {
    return {offset + additional_offset, size};
  }

  void Unite(const LogicalRect&);
  void UniteEvenIfEmpty(const LogicalRect&);

  // You can use this function only if we know `rect` is logical. See also:
  //  * `EnclosingLayoutRect() -> LayoutRect`
  //  * `PhysicalRect::EnclosingRect() -> PhysicalRect`
  static LogicalRect EnclosingRect(const gfx::RectF& rect) {
    const LogicalOffset offset(LayoutUnit::FromFloatFloor(rect.x()),
                               LayoutUnit::FromFloatFloor(rect.y()));
    const LogicalSize size(
        LayoutUnit::FromFloatCeil(rect.right()) - offset.inline_offset,
        LayoutUnit::FromFloatCeil(rect.bottom()) - offset.block_offset);
    return LogicalRect(offset, size);
  }

  String ToString() const;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const LogicalRect&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_LOGICAL_RECT_H_
