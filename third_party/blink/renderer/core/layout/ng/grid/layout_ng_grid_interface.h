// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_LAYOUT_NG_GRID_INTERFACE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_LAYOUT_NG_GRID_INTERFACE_H_

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/table/interface_casting.h"

namespace blink {

class LayoutUnit;

// This class provides an abstraction between legacy and NG grid. This allows us
// to avoid forking behavior elsewhere.
class LayoutNGGridInterface {
 public:
  virtual wtf_size_t ExplicitGridStartForDirection(
      GridTrackSizingDirection direction) const = 0;
  virtual wtf_size_t ExplicitGridEndForDirection(
      GridTrackSizingDirection direction) const = 0;

  virtual wtf_size_t AutoRepeatCountForDirection(
      GridTrackSizingDirection direction) const = 0;
  virtual Vector<LayoutUnit, 1> TrackSizesForComputedStyle(
      GridTrackSizingDirection direction) const = 0;
  virtual Vector<LayoutUnit> RowPositions() const = 0;
  virtual Vector<LayoutUnit> ColumnPositions() const = 0;
  virtual LayoutUnit GridGap(GridTrackSizingDirection) const = 0;
  virtual LayoutUnit GridItemOffset(GridTrackSizingDirection) const = 0;
};

template <>
struct InterfaceDowncastTraits<LayoutNGGridInterface> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsLayoutGridIncludingNG();
  }
  static const LayoutNGGridInterface& ConvertFrom(const LayoutObject& object) {
    return *object.ToLayoutNGGridInterface();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_LAYOUT_NG_GRID_INTERFACE_H_
