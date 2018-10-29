// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalRect_h
#define NGPhysicalRect_h

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_physical_location.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_physical_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

// NGPixelSnappedPhysicalRect is the position and size of a rect relative to the
// root document snapped to device pixels.
struct CORE_EXPORT NGPixelSnappedPhysicalRect {
  int top;
  int left;
  int width;
  int height;
};

// NGPhysicalRect is the position and size of a rect (typically a fragment)
// relative to the root document.
struct CORE_EXPORT NGPhysicalRect {
  NGPhysicalRect();
  NGPhysicalRect(const NGPhysicalLocation& location, const NGPhysicalSize& size)
      : location(location), size(size) {}

  NGPhysicalLocation Location() const { return location; }
  NGPhysicalSize Size() const { return size; }
  NGPixelSnappedPhysicalRect SnapToDevicePixels() const;

  NGPhysicalLocation location;
  NGPhysicalSize size;

  bool operator==(const NGPhysicalRect& other) const;

  String ToString() const;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGPhysicalRect&);

}  // namespace blink

#endif  // NGPhysicalRect_h
