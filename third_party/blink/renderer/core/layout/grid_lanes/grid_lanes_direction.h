// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_DIRECTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_DIRECTION_H_

namespace blink {

// Types used to keep track of the current value of the 'grid-lanes-direction'
// property with syntax `grid-lanes-direction: normal | [ row | column ] [
// fill-reverse || track-reverse ]?`.
//
// https://github.com/w3c/csswg-drafts/issues/12803#issuecomment-3643945412
//
// TODO(almaher): Add actual link to spec once we have a resolution one way or
// another.
enum GridLanesOrientation { kNormal, kRow, kColumn };

struct GridLanesDirection {
  explicit GridLanesDirection() = default;

  GridLanesDirection(GridLanesOrientation orientation,
                     bool is_fill_reverse,
                     bool is_track_reverse)
      : orientation(orientation),
        is_fill_reverse(is_fill_reverse),
        is_track_reverse(is_track_reverse) {
    // The 'normal' keyword cannot be paired with either reverse keyword.
    if (orientation == kNormal) {
      CHECK(!is_fill_reverse);
      CHECK(!is_track_reverse);
    }
  }

  bool operator==(const GridLanesDirection& other) const = default;
  bool operator!=(const GridLanesDirection& other) const = default;

  GridLanesOrientation orientation = GridLanesOrientation::kNormal;
  bool is_fill_reverse = false;
  bool is_track_reverse = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_DIRECTION_H_
