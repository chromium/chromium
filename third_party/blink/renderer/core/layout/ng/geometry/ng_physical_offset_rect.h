// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalOffsetRect_h
#define NGPhysicalOffsetRect_h

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_physical_offset.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_physical_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class ComputedStyle;
class FloatRect;
class LayoutRect;
struct NGPhysicalBoxStrut;

// NGPhysicalOffsetRect is the position and size of a rect (typically a
// fragment) relative to its parent rect in the physical coordinate system.
struct CORE_EXPORT NGPhysicalOffsetRect {
  NGPhysicalOffsetRect() = default;
  NGPhysicalOffsetRect(const NGPhysicalOffset& offset,
                       const NGPhysicalSize& size)
      : offset(offset), size(size) {}

  NGPhysicalOffset offset;
  NGPhysicalSize size;

  bool IsEmpty() const { return size.IsEmpty(); }
  LayoutUnit Right() const { return offset.left + size.width; }
  LayoutUnit Bottom() const { return offset.top + size.height; }

  bool operator==(const NGPhysicalOffsetRect& other) const;

  NGPhysicalOffsetRect operator+(const NGPhysicalOffset&) const;

  void Unite(const NGPhysicalOffsetRect&);
  void UniteIfNonZero(const NGPhysicalOffsetRect&);
  void UniteEvenIfEmpty(const NGPhysicalOffsetRect&);

  void Expand(const NGPhysicalBoxStrut&);

  // Conversions from/to existing code. New code prefers type safety for
  // logical/physical distinctions.
  explicit NGPhysicalOffsetRect(const LayoutRect&);
  LayoutRect ToLayoutRect() const;
  LayoutRect ToLayoutFlippedRect(const ComputedStyle&,
                                 const NGPhysicalSize&) const;

  FloatRect ToFloatRect() const;

  String ToString() const;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&,
                                     const NGPhysicalOffsetRect&);

}  // namespace blink

#endif  // NGPhysicalOffsetRect_h
