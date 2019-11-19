// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GEOMETRY_NG_BFC_OFFSET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GEOMETRY_NG_BFC_OFFSET_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

struct CORE_EXPORT NGBfcDelta {
  NGBfcDelta() = default;
  NGBfcDelta(LayoutUnit line_offset_delta, LayoutUnit block_offset_delta)
      : line_offset_delta(line_offset_delta),
        block_offset_delta(block_offset_delta) {}

  LayoutUnit line_offset_delta;
  LayoutUnit block_offset_delta;
};

// NGBfcOffset is the position of a rect (typically a fragment) relative to
// a block formatting context (BFC). BFCs are agnostic to text direction, and
// uses line_offset instead of inline_offset.
//
// Care must be taken when converting this to a LogicalOffset to respect the
// text direction.
struct CORE_EXPORT NGBfcOffset {
  NGBfcOffset() = default;
  NGBfcOffset(LayoutUnit line_offset, LayoutUnit block_offset)
      : line_offset(line_offset), block_offset(block_offset) {}

  LayoutUnit line_offset;
  LayoutUnit block_offset;

  NGBfcOffset& operator+=(const NGBfcDelta& delta) {
    *this = *this + delta;
    return *this;
  }

  NGBfcOffset operator+(const NGBfcDelta& delta) {
    return {line_offset + delta.line_offset_delta,
            block_offset + delta.block_offset_delta};
  }

  bool operator==(const NGBfcOffset& other) const {
    return std::tie(other.line_offset, other.block_offset) ==
           std::tie(line_offset, block_offset);
  }

  bool operator!=(const NGBfcOffset& other) const { return !operator==(other); }

  String ToString() const;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGBfcOffset&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GEOMETRY_NG_BFC_OFFSET_H_
