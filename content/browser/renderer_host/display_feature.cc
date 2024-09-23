// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/display_feature.h"

namespace content {

bool DisplayFeature::operator==(const DisplayFeature& other) const {
  return orientation == other.orientation && offset == other.offset &&
         mask_length == other.mask_length;
}

bool DisplayFeature::operator!=(const DisplayFeature& other) const {
  return !(*this == other);
}

std::vector<gfx::Rect> DisplayFeature::ComputeViewportSegments(
    const gfx::Size& visible_viewport_size,
    int root_view_offset_from_origin) const {
  std::vector<gfx::Rect> viewport_segments;

  int display_feature_end = offset + mask_length;
  if (orientation == DisplayFeature::Orientation::kVertical) {
    // If the display feature is vertically oriented, it splits or masks
    // the widget into two side-by-side segments. Note that in the masking
    // scenario, there is an area of the widget that are not covered by the
    // union of the viewport segments - this area's pixels will not be visible
    // to the user.
    viewport_segments.emplace_back(0, 0, offset,
                                   visible_viewport_size.height());
    viewport_segments.emplace_back(
        display_feature_end, 0,
        visible_viewport_size.width() - display_feature_end,
        visible_viewport_size.height());
  } else {
    // If the display feature is offset in the y direction, it splits or masks
    // the widget into two stacked segments.
    // We need to offset the display feature by the browser controls top height.
    display_feature_end = display_feature_end - root_view_offset_from_origin;
    int final_offset = offset - root_view_offset_from_origin;
    if (final_offset < 0 || display_feature_end < 0) {
      return viewport_segments;
    }
    viewport_segments.emplace_back(0, 0, visible_viewport_size.width(),
                                   final_offset);
    viewport_segments.emplace_back(
        0, display_feature_end, visible_viewport_size.width(),
        visible_viewport_size.height() - display_feature_end);
  }

  return viewport_segments;
}

// static
std::optional<DisplayFeature> DisplayFeature::Create(Orientation orientation,
                                                     int offset,
                                                     int mask_length,
                                                     int width,
                                                     int height,
                                                     ParamErrorEnum* error) {
  if (!width && !height) {
    *error = ParamErrorEnum::kDisplayFeatureWithZeroScreenSize;
    return std::nullopt;
  }

  if (offset < 0 || mask_length < 0) {
    *error = ParamErrorEnum::kNegativeDisplayFeatureParams;
    return std::nullopt;
  }

  if (orientation == Orientation::kVertical && offset + mask_length > width) {
    *error = ParamErrorEnum::kOutsideScreenWidth;
    return std::nullopt;
  }

  if (orientation == Orientation::kHorizontal &&
      offset + mask_length > height) {
    *error = ParamErrorEnum::kOutsideScreenHeight;
    return std::nullopt;
  }

  return DisplayFeature{orientation, offset, mask_length};
}

}  // namespace content
