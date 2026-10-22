// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_TRACK_OPENING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_TRACK_OPENING_H_

#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// TODO(javiercon): Move `GridLanesRunningPositions::TrackOpening`, and the
// data structures it depends on, to this file.
//
// The start and end of a track opening along the stacking axis.
struct TrackOpeningCoordinates {
  DISALLOW_NEW();

 public:
  LayoutUnit Size() const { return end_position - start_position; }

  bool operator==(const TrackOpeningCoordinates&) const = default;

  LayoutUnit start_position;
  LayoutUnit end_position;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_TRACK_OPENING_H_
