// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/coordinate_conversion.h"

namespace remoting::protocol {

namespace {

float CalculateFractionalCoordinate(int val, int size) {
  if (size <= 1) {
    return 0.f;
  }
  // Clamp to prevent bogus values, in case the coordinates are out-of-sync with
  // the display config.
  return std::clamp(static_cast<float>(val) / (size - 1), 0.f, 1.f);
}

}  // namespace

FractionalCoordinate ToFractionalCoordinate(
    webrtc::ScreenId screen_id,
    const webrtc::DesktopSize& screen_size,
    const webrtc::DesktopVector& coordinate) {
  FractionalCoordinate fractional;
  fractional.set_screen_id(screen_id);
  fractional.set_x(
      CalculateFractionalCoordinate(coordinate.x(), screen_size.width()));
  fractional.set_y(
      CalculateFractionalCoordinate(coordinate.y(), screen_size.height()));
  return fractional;
}

}  // namespace remoting::protocol
