// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/x11_display_util.h"

#include <stdint.h>

#include "base/numerics/safe_conversions.h"
#include "ui/gfx/geometry/vector2d.h"

namespace remoting {

constexpr int kDefaultScreenDpi = 96;
constexpr double kMillimetersPerInch = 25.4;

int CalculateDpi(uint16_t length_in_pixels, uint32_t length_in_mm) {
  if (length_in_mm == 0) {
    return kDefaultScreenDpi;
  }
  double pixels_per_mm = static_cast<double>(length_in_pixels) / length_in_mm;
  double pixels_per_inch = pixels_per_mm * kMillimetersPerInch;
  return base::ClampRound(pixels_per_inch);
}

gfx::Vector2d GetMonitorDpi(const x11::RandR::MonitorInfo& monitor) {
  return gfx::Vector2d(
      CalculateDpi(monitor.width, monitor.width_in_millimeters),
      CalculateDpi(monitor.height, monitor.height_in_millimeters));
}

x11::RandRMonitorConfig ToVideoTrackLayout(
    const x11::RandR::MonitorInfo& monitor) {
  return x11::RandRMonitorConfig(
      static_cast<x11::RandRMonitorConfig::ScreenId>(monitor.name),
      gfx::Rect(monitor.x, monitor.y, monitor.width, monitor.height),
      GetMonitorDpi(monitor));
}

}  // namespace remoting
