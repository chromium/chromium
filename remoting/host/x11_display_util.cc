// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/x11_display_util.h"

#include <stdint.h>

#include "base/numerics/safe_conversions.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

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

webrtc::DesktopVector GetMonitorDpi(const x11::RandR::MonitorInfo& monitor) {
  return webrtc::DesktopVector(
      CalculateDpi(monitor.width, monitor.width_in_millimeters),
      CalculateDpi(monitor.height, monitor.height_in_millimeters));
}

protocol::VideoTrackLayout ToVideoTrackLayout(
    const x11::RandR::MonitorInfo& monitor) {
  protocol::VideoTrackLayout layout;
  layout.set_screen_id(static_cast<webrtc::ScreenId>(monitor.name));
  layout.set_position_x(monitor.x);
  layout.set_position_y(monitor.y);
  layout.set_width(monitor.width);
  layout.set_height(monitor.height);
  webrtc::DesktopVector dpi = GetMonitorDpi(monitor);
  layout.set_x_dpi(dpi.x());
  layout.set_y_dpi(dpi.y());
  return layout;
}

}  // namespace remoting
