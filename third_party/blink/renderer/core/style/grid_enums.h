// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GRID_ENUMS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GRID_ENUMS_H_

namespace blink {

enum GridPositionSide {
  kColumnStartSide,
  kColumnEndSide,
  kRowStartSide,
  kRowEndSide
};

enum GridTrackSizingDirection { kForColumns, kForRows };

constexpr GridTrackSizingDirection OppositeDirection(
    GridTrackSizingDirection direction) {
  return direction == kForColumns ? kForRows : kForColumns;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GRID_ENUMS_H_
