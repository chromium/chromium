// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLogicalSize_h
#define NGLogicalSize_h

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_logical_offset.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

struct NGLogicalOffset;
struct NGPhysicalSize;
#define NGSizeIndefinite LayoutUnit(-1)

// NGLogicalSize is the size of rect (typically a fragment) in the logical
// coordinate system.
struct CORE_EXPORT NGLogicalSize {
  NGLogicalSize() = default;
  NGLogicalSize(LayoutUnit inline_size, LayoutUnit block_size)
      : inline_size(inline_size), block_size(block_size) {}

  LayoutUnit inline_size;
  LayoutUnit block_size;

  NGPhysicalSize ConvertToPhysical(WritingMode mode) const;
  bool operator==(const NGLogicalSize& other) const;

  bool IsEmpty() const {
    return inline_size == LayoutUnit() || block_size == LayoutUnit();
  }

  void Flip() { std::swap(inline_size, block_size); }
};

inline NGLogicalSize& operator-=(NGLogicalSize& a, const NGBoxStrut& b) {
  a.inline_size -= b.InlineSum();
  a.block_size -= b.BlockSum();
  return a;
}

CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGLogicalSize&);

// NGLogicalDelta resolves the ambiguity of subtractions.
//
// "offset - offset" is ambiguous because both of below are true:
//   offset + offset = offset
//   offset + size = offset
//
// NGLogicalDelta resolves this ambiguity by allowing implicit conversions both
// to NGLogicalOffset and to NGLogicalSize.
struct CORE_EXPORT NGLogicalDelta : public NGLogicalSize {
 public:
  using NGLogicalSize::NGLogicalSize;
  operator NGLogicalOffset() const { return {inline_size, block_size}; }
};

}  // namespace blink

#endif  // NGLogicalSize_h
