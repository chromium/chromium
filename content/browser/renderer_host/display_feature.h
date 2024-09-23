// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DISPLAY_FEATURE_H_
#define CONTENT_BROWSER_RENDERER_HOST_DISPLAY_FEATURE_H_

#include <optional>
#include <vector>

#include "build/build_config.h"
#include "content/common/content_export.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

// Information about a physical attribute of the screen which that creates a
// Logical separator or divider (e.g. a fold or mask).
// This is a visual example of a vertically oriented display feature that masks
// content underneath
//
//    Orientation: vertical
//
//                 offset
//                   |
//         +---------|===|---------+
//         |         |   |         |
//         |         |   |         |
//         |         |   |         |
//         |         |   |         |
//         |         |   |         |
//         +---------|===|---------+
//                      \
//                      mask_length
//
// Note that the implicit height of the display feature is the entire height of
// the screen on which it exists.
struct CONTENT_EXPORT DisplayFeature {
  enum class Orientation { kVertical, kHorizontal, kMaxValue = kHorizontal };
  enum class ParamErrorEnum {
    kDisplayFeatureWithZeroScreenSize = 1,
    kNegativeDisplayFeatureParams,
    kOutsideScreenWidth,
    kOutsideScreenHeight
  };

  // The orientation of the display feature in relation to the screen.
  Orientation orientation = Orientation::kVertical;

  // The offset from the screen origin in either the x (for vertical
  // orientation) or y (for horizontal orientation) direction.
  int offset = 0;

  // A display feature may mask content such that it is not physically
  // displayed - this length along with the offset describes this area.
  // A display feature that only splits content will have a 0 |mask_length|.
  int mask_length = 0;

  bool operator==(const DisplayFeature& other) const;
  bool operator!=(const DisplayFeature& other) const;

  // Computes logical segments of the |visible_viewport_size|, based on
  // this display feature. These segments are in DIPs relative to the widget
  // origin.
  std::vector<gfx::Rect> ComputeViewportSegments(
      const gfx::Size& visible_viewport_size,
      int root_view_offset_from_origin = 0) const;

  static std::optional<DisplayFeature> Create(
      Orientation orientation,
      int offset,
      int mask_length,
      int screen_width,
      int screen_height,
      DisplayFeature::ParamErrorEnum* error);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DISPLAY_FEATURE_H_
