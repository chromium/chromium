// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_properties.h"

namespace blink {

bool NGGridProperties::HasBaseline(
    const GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns)
             ? (has_baseline_column ||
                (has_orthogonal_item && has_baseline_row))
             : (has_baseline_row ||
                (has_orthogonal_item && has_baseline_column));
}

bool NGGridProperties::HasFlexibleTrack(
    const GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns)
             ? column_properties.HasProperty(
                   TrackSpanProperties::kHasFlexibleTrack)
             : row_properties.HasProperty(
                   TrackSpanProperties::kHasFlexibleTrack);
}

bool NGGridProperties::HasIntrinsicTrack(
    const GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns)
             ? column_properties.HasProperty(
                   TrackSpanProperties::kHasIntrinsicTrack)
             : row_properties.HasProperty(
                   TrackSpanProperties::kHasIntrinsicTrack);
}

bool NGGridProperties::IsDependentOnAvailableSize(
    const GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns)
             ? column_properties.HasProperty(
                   TrackSpanProperties::kIsDependentOnAvailableSize)
             : row_properties.HasProperty(
                   TrackSpanProperties::kIsDependentOnAvailableSize);
}

bool NGGridProperties::IsSpanningOnlyDefiniteTracks(
    const GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns)
             ? !column_properties.HasProperty(
                   TrackSpanProperties::kHasNonDefiniteTrack)
             : !row_properties.HasProperty(
                   TrackSpanProperties::kHasNonDefiniteTrack);
}

}  // namespace blink
