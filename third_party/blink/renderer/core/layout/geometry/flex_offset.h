// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_FLEX_OFFSET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_FLEX_OFFSET_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace WTF {
class String;
}  // namespace WTF

namespace blink {

struct LogicalOffset;

// FlexOffset is the position of a flex item relative to its parent flexbox
// in the main axis/cross axis. For more information, see:
// https://drafts.csswg.org/css-flexbox-1/#box-model
struct CORE_EXPORT FlexOffset {
  constexpr FlexOffset() = default;
  constexpr FlexOffset(LayoutUnit main_axis_offset,
                       LayoutUnit cross_axis_offset)
      : main_axis_offset(main_axis_offset),
        cross_axis_offset(cross_axis_offset) {}

  LayoutUnit main_axis_offset;
  LayoutUnit cross_axis_offset;

  constexpr bool operator==(const FlexOffset& other) const {
    return main_axis_offset == other.main_axis_offset &&
           cross_axis_offset == other.cross_axis_offset;
  }
  constexpr bool operator!=(const FlexOffset& other) const {
    return !operator==(other);
  }

  FlexOffset operator+(const FlexOffset& other) const {
    return {main_axis_offset + other.main_axis_offset,
            cross_axis_offset + other.cross_axis_offset};
  }

  FlexOffset& operator+=(const FlexOffset& other) {
    *this = *this + other;
    return *this;
  }

  FlexOffset& operator-=(const FlexOffset& other) {
    main_axis_offset -= other.main_axis_offset;
    cross_axis_offset -= other.cross_axis_offset;
    return *this;
  }

  FlexOffset TransposedOffset() const {
    return FlexOffset(cross_axis_offset, main_axis_offset);
  }

  LogicalOffset ToLogicalOffset(bool is_column_flex_container) const;

  WTF::String ToString() const;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const FlexOffset&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_FLEX_OFFSET_H_
