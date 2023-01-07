// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/wayland_display_info.h"

namespace remoting {

DisplayInfo::DisplayInfo() = default;

DisplayInfo::DisplayInfo(int x,
                         int y,
                         int transform,
                         int physical_width,
                         int physical_height,
                         int subpixel)
    : x(x),
      y(y),
      transform(transform),
      physical_width(physical_width),
      physical_height(physical_height),
      subpixel(subpixel) {}

DisplayInfo::DisplayInfo(int width, int height, int refresh)
    : width(width), height(height), refresh(refresh) {}

DisplayInfo::DisplayInfo(int scale_factor) : scale_factor(scale_factor) {}

DisplayInfo::DisplayInfo(const DisplayInfo&) = default;
DisplayInfo::DisplayInfo(DisplayInfo&&) = default;
DisplayInfo& DisplayInfo::operator=(const DisplayInfo&) = default;
DisplayInfo& DisplayInfo::operator=(DisplayInfo&) = default;
DisplayInfo::~DisplayInfo() = default;

}  // namespace remoting
