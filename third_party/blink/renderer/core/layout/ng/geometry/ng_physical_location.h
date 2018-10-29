// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalLocation_h
#define NGPhysicalLocation_h

#include "third_party/blink/renderer/core/core_export.h"

#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

// NGPhysicalLocation is the position of a rect (typically a fragment) relative
// to the root document.
struct CORE_EXPORT NGPhysicalLocation {
  NGPhysicalLocation() = default;
  NGPhysicalLocation(LayoutUnit left, LayoutUnit top) : left(left), top(top) {}
  LayoutUnit left;
  LayoutUnit top;

  bool operator==(const NGPhysicalLocation& other) const;

  String ToString() const;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGPhysicalLocation&);

}  // namespace blink

#endif  // NGPhysicalLocation_h
