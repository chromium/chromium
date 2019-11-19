// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GEOMETRY_NG_BOX_STRUT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GEOMETRY_NG_BOX_STRUT_H_

#include <utility>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect_outsets.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

struct NGLineBoxStrut;
struct NGPhysicalBoxStrut;

// This struct is used for storing margins, borders or padding of a box on all
// four edges.
struct CORE_EXPORT NGBoxStrut {
  NGBoxStrut() = default;
  NGBoxStrut(LayoutUnit inline_start,
             LayoutUnit inline_end,
             LayoutUnit block_start,
             LayoutUnit block_end)
      : inline_start(inline_start),
        inline_end(inline_end),
        block_start(block_start),
        block_end(block_end) {}
  NGBoxStrut(const NGLineBoxStrut&, bool is_flipped_lines);

  LayoutUnit LineLeft(TextDirection direction) const {
    return IsLtr(direction) ? inline_start : inline_end;
  }
  LayoutUnit LineRight(TextDirection direction) const {
    return IsLtr(direction) ? inline_end : inline_start;
  }

  LayoutUnit InlineSum() const { return inline_start + inline_end; }
  LayoutUnit BlockSum() const { return block_start + block_end; }

  LogicalOffset StartOffset() const { return {inline_start, block_start}; }

  bool IsEmpty() const { return *this == NGBoxStrut(); }

  inline NGPhysicalBoxStrut ConvertToPhysical(WritingMode, TextDirection) const;

  // The following two operators exist primarily to have an easy way to access
  // the sum of border and padding.
  NGBoxStrut& operator+=(const NGBoxStrut& other) {
    inline_start += other.inline_start;
    inline_end += other.inline_end;
    block_start += other.block_start;
    block_end += other.block_end;
    return *this;
  }

  NGBoxStrut operator+(const NGBoxStrut& other) const {
    NGBoxStrut result(*this);
    result += other;
    return result;
  }

  NGBoxStrut& operator-=(const NGBoxStrut& other) {
    inline_start -= other.inline_start;
    inline_end -= other.inline_end;
    block_start -= other.block_start;
    block_end -= other.block_end;
    return *this;
  }

  NGBoxStrut operator-(const NGBoxStrut& other) const {
    NGBoxStrut result(*this);
    result -= other;
    return result;
  }

  bool operator==(const NGBoxStrut& other) const {
    return std::tie(other.inline_start, other.inline_end, other.block_start,
                    other.block_end) ==
           std::tie(inline_start, inline_end, block_start, block_end);
  }
  bool operator!=(const NGBoxStrut& other) const { return !(*this == other); }

  String ToString() const;

  LayoutUnit inline_start;
  LayoutUnit inline_end;
  LayoutUnit block_start;
  LayoutUnit block_end;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGBoxStrut&);

// A variant of NGBoxStrut in the line-relative coordinate system.
//
// 'line-over' is 'block-start' and 'line-under' is 'block-end' unless it is in
// flipped-lines writing-mode (i.e., 'vertical-lr'), in which case they are
// swapped.
//
// https://drafts.csswg.org/css-writing-modes-3/#line-mappings
struct CORE_EXPORT NGLineBoxStrut {
  NGLineBoxStrut() = default;
  NGLineBoxStrut(LayoutUnit inline_start,
                 LayoutUnit inline_end,
                 LayoutUnit line_over,
                 LayoutUnit line_under)
      : inline_start(inline_start),
        inline_end(inline_end),
        line_over(line_over),
        line_under(line_under) {}
  NGLineBoxStrut(const NGBoxStrut&, bool is_flipped_lines);

  LayoutUnit InlineSum() const { return inline_start + inline_end; }
  LayoutUnit BlockSum() const { return line_over + line_under; }

  bool operator==(const NGLineBoxStrut& other) const {
    return inline_start == other.inline_start &&
           inline_end == other.inline_end && line_over == other.line_over &&
           line_under == other.line_under;
  }

  LayoutUnit inline_start;
  LayoutUnit inline_end;
  LayoutUnit line_over;
  LayoutUnit line_under;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGLineBoxStrut&);

// Struct to store pixel snapped physical dimensions.
struct CORE_EXPORT NGPixelSnappedPhysicalBoxStrut {
  NGPixelSnappedPhysicalBoxStrut() = default;
  NGPixelSnappedPhysicalBoxStrut(int top, int right, int bottom, int left)
      : top(top), right(right), bottom(bottom), left(left) {}
  int top;
  int right;
  int bottom;
  int left;
};

// Struct to store physical dimensions, independent of writing mode and
// direction.
// See https://drafts.csswg.org/css-writing-modes-3/#abstract-box
struct CORE_EXPORT NGPhysicalBoxStrut {
  NGPhysicalBoxStrut() = default;
  NGPhysicalBoxStrut(LayoutUnit top,
                     LayoutUnit right,
                     LayoutUnit bottom,
                     LayoutUnit left)
      : top(top), right(right), bottom(bottom), left(left) {}

  // Converts physical dimensions to logical ones per
  // https://drafts.csswg.org/css-writing-modes-3/#logical-to-physical
  NGBoxStrut ConvertToLogical(WritingMode writing_mode,
                              TextDirection direction) const {
    NGBoxStrut strut;
    switch (writing_mode) {
      case WritingMode::kHorizontalTb:
        strut = {left, right, top, bottom};
        break;
      case WritingMode::kVerticalRl:
      case WritingMode::kSidewaysRl:
        strut = {top, bottom, right, left};
        break;
      case WritingMode::kVerticalLr:
        strut = {top, bottom, left, right};
        break;
      case WritingMode::kSidewaysLr:
        strut = {bottom, top, left, right};
        break;
    }
    if (direction == TextDirection::kRtl)
      std::swap(strut.inline_start, strut.inline_end);
    return strut;
  }

  // Converts physical dimensions to line-relative logical ones per
  // https://drafts.csswg.org/css-writing-modes-3/#line-directions
  NGLineBoxStrut ConvertToLineLogical(WritingMode writing_mode,
                                      TextDirection direction) const {
    return NGLineBoxStrut(ConvertToLogical(writing_mode, direction),
                          IsFlippedLinesWritingMode(writing_mode));
  }

  NGPixelSnappedPhysicalBoxStrut SnapToDevicePixels() const {
    return NGPixelSnappedPhysicalBoxStrut(top.Round(), right.Round(),
                                          bottom.Round(), left.Round());
  }

  LayoutUnit HorizontalSum() const { return left + right; }
  LayoutUnit VerticalSum() const { return top + bottom; }

  LayoutRectOutsets ToLayoutRectOutsets() const {
    return LayoutRectOutsets(top, right, bottom, left);
  }

  bool operator==(const NGPhysicalBoxStrut& other) const {
    return top == other.top && right == other.right && bottom == other.bottom &&
           left == other.left;
  }

  bool IsZero() const { return !top && !right && !bottom && !left; }

  LayoutUnit top;
  LayoutUnit right;
  LayoutUnit bottom;
  LayoutUnit left;
};

inline NGPhysicalBoxStrut NGBoxStrut::ConvertToPhysical(
    WritingMode writing_mode,
    TextDirection direction) const {
  LayoutUnit direction_start = inline_start;
  LayoutUnit direction_end = inline_end;
  if (direction == TextDirection::kRtl)
    std::swap(direction_start, direction_end);
  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      return NGPhysicalBoxStrut(block_start, direction_end, block_end,
                                direction_start);
    case WritingMode::kVerticalRl:
    case WritingMode::kSidewaysRl:
      return NGPhysicalBoxStrut(direction_start, block_start, direction_end,
                                block_end);
    case WritingMode::kVerticalLr:
      return NGPhysicalBoxStrut(direction_start, block_end, direction_end,
                                block_start);
    case WritingMode::kSidewaysLr:
      return NGPhysicalBoxStrut(direction_end, block_end, direction_start,
                                block_start);
    default:
      NOTREACHED();
      return NGPhysicalBoxStrut();
  }
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GEOMETRY_NG_BOX_STRUT_H_
