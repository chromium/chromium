// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MASONRY_LAYOUT_MASONRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MASONRY_LAYOUT_MASONRY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class GridLayoutData;

class CORE_EXPORT LayoutMasonry : public LayoutBlock {
 public:
  explicit LayoutMasonry(Element* element);

  const char* GetName() const override {
    NOT_DESTROYED();
    // This string can affect a production behavior.
    // See tool_highlight.ts in devtools-frontend.
    return "LayoutMasonry";
  }

  Vector<LayoutUnit> GridTrackPositions(
      GridTrackSizingDirection track_direction) const;
  LayoutUnit GridGap(GridTrackSizingDirection track_direction) const;
  LayoutUnit MasonryItemOffset(GridTrackSizingDirection track_direction) const;

  const GridLayoutData* LayoutData() const;

 private:
  bool IsLayoutMasonry() const final {
    NOT_DESTROYED();
    return true;
  }

  // TODO(almaher): There are a bunch of helpers we are missing that are in
  // LayoutGrid. Do we need those, too, or should we be based on LayoutGrid
  // instead of LayoutBlock?
};

// wtf/casting.h helper.
template <>
struct DowncastTraits<LayoutMasonry> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsLayoutMasonry();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MASONRY_LAYOUT_MASONRY_H_
