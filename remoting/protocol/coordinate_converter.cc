// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/coordinate_converter.h"

#include <algorithm>
#include <optional>

#include "base/check.h"
#include "base/numerics/safe_conversions.h"
#include "remoting/base/logging.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/coordinates.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting::protocol {

namespace {

// Scales `fraction` (between 0 and 1) to a value between `minimum` and
// (minimum + size - 1).
int ScaleAndClamp(float fraction, int minimum, int size) {
  int scaled = base::ClampRound(fraction * size);
  scaled = std::clamp(scaled, 0, size - 1);
  return scaled + minimum;
}

}  // namespace

CoordinateConverter::CoordinateConverter() = default;
CoordinateConverter::~CoordinateConverter() = default;

void CoordinateConverter::set_video_layout(const VideoLayout& layout) {
  video_layout_ = layout;
}

std::optional<webrtc::DesktopVector>
CoordinateConverter::ToGlobalAbsoluteCoordinate(
    const FractionalCoordinate& fractional) const {
  if (!fractional.has_x() || !fractional.has_y()) {
    LOG(ERROR) << "Event has incomplete fractional coordinates.";
    return std::nullopt;
  }

  if (!fractional.has_screen_id()) {
    LOG(ERROR) << "Fractional coordinates have no screen_id.";
    return std::nullopt;
  }
  auto screen_id = fractional.screen_id();
  VLOG(3) << "screen_id = " << screen_id;
  auto it = std::ranges::find_if(
      video_layout_.video_track(), [screen_id](const VideoTrackLayout& track) {
        return track.has_screen_id() && track.screen_id() == screen_id;
      });
  if (it == video_layout_.video_track().end()) {
    LOG(ERROR) << "screen_id " << screen_id
               << " not found in the video layout.";
    return std::nullopt;
  }

  const VideoTrackLayout& monitor = *it;

  DCHECK(monitor.has_position_x());
  DCHECK(monitor.has_position_y());
  DCHECK(monitor.has_width());
  DCHECK(monitor.has_height());
  const int bounds_x = monitor.position_x();
  const int bounds_y = monitor.position_y();
  const int bounds_width = monitor.width();
  const int bounds_height = monitor.height();

  int new_x = ScaleAndClamp(fractional.x(), bounds_x, bounds_width);
  int new_y = ScaleAndClamp(fractional.y(), bounds_y, bounds_height);
  VLOG(3) << "(" << fractional.x() << ", " << fractional.y() << ") -> ("
          << new_x << ", " << new_y << ")";
  return webrtc::DesktopVector{new_x, new_y};
}

}  // namespace remoting::protocol
