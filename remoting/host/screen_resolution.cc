// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/screen_resolution.h"

#include <stdint.h>

#include <algorithm>
#include <limits>

#include "base/check_op.h"

namespace remoting {

ScreenResolution::ScreenResolution()
    : dimensions_(webrtc::DesktopSize(0, 0)),
      dpi_(webrtc::DesktopVector(0, 0)) {
}

ScreenResolution::ScreenResolution(const webrtc::DesktopSize& dimensions,
                                   const webrtc::DesktopVector& dpi)
    : dimensions_(dimensions),
      dpi_(dpi) {
  // Check that dimensions are not negative.
  DCHECK(!dimensions.is_empty() || dimensions.equals(webrtc::DesktopSize()));
  DCHECK_GE(dpi.x(), 0);
  DCHECK_GE(dpi.y(), 0);
}

webrtc::DesktopSize ScreenResolution::ScaleDimensionsToDpi(
    const webrtc::DesktopVector& new_dpi) const {
  int64_t width = dimensions_.width();
  int64_t height = dimensions_.height();

  // Scale the screen dimensions to new DPI.
  width = std::min(width * new_dpi.x() / dpi_.x(),
                   static_cast<int64_t>(std::numeric_limits<int32_t>::max()));
  height = std::min(height * new_dpi.y() / dpi_.y(),
                    static_cast<int64_t>(std::numeric_limits<int32_t>::max()));
  return webrtc::DesktopSize(width, height);
}

bool ScreenResolution::IsEmpty() const {
  return dimensions_.is_empty() || dpi_.is_zero();
}

bool ScreenResolution::Equals(const ScreenResolution& other) const {
  return dimensions_.equals(other.dimensions()) && dpi_.equals(other.dpi());
}

}  // namespace remoting
