// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/screen_info.h"

namespace content {

ScreenInfo::ScreenInfo() = default;
ScreenInfo::ScreenInfo(const ScreenInfo& other) = default;
ScreenInfo::~ScreenInfo() = default;

bool ScreenInfo::operator==(const ScreenInfo& other) const {
  return device_scale_factor == other.device_scale_factor &&
         color_space == other.color_space && depth == other.depth &&
         depth_per_component == other.depth_per_component &&
         is_monochrome == other.is_monochrome &&
         display_frequency == other.display_frequency && rect == other.rect &&
         available_rect == other.available_rect &&
         orientation_type == other.orientation_type &&
         orientation_angle == other.orientation_angle;
}

bool ScreenInfo::operator!=(const ScreenInfo& other) const {
  return !(*this == other);
}


}  // namespace content
