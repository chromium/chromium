// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fractional_input_filter.h"

#include <algorithm>

#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "remoting/base/logging.h"
#include "remoting/proto/event.pb.h"

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

FractionalInputFilter::FractionalInputFilter() = default;

FractionalInputFilter::FractionalInputFilter(InputStub* input_stub)
    : InputFilter(input_stub) {}

FractionalInputFilter::~FractionalInputFilter() = default;

void FractionalInputFilter::set_video_layout(const VideoLayout& layout) {
  video_layout_ = layout;
}

void FractionalInputFilter::set_fallback_geometry(
    webrtc::DesktopRect geometry) {
  fallback_geometry_ = geometry;
}

void FractionalInputFilter::InjectMouseEvent(const MouseEvent& event) {
  if (!event.has_fractional_coordinate()) {
    InputFilter::InjectMouseEvent(event);
    return;
  }

  int new_x;
  int new_y;
  if (ComputeXY(new_x, new_y, event)) {
    MouseEvent new_event(event);
    new_event.set_x(new_x);
    new_event.set_y(new_y);
    InputFilter::InjectMouseEvent(new_event);
  }
}

void FractionalInputFilter::InjectTouchEvent(const TouchEvent& event) {
  // Copy the event, so it can be mutated. This could be optimized for cases
  // where mutation is not needed. But in the longer term, the TouchEvents will
  // all have fractional coordinates, and then a copy is needed anyway.
  TouchEvent new_event(event);

  for (TouchEventPoint& touch_point : *(new_event.mutable_touch_points())) {
    // Events with no fractional-coordinates should be passed through unchanged.
    if (touch_point.has_fractional_coordinate()) {
      int new_x;
      int new_y;
      if (!ComputeXY(new_x, new_y, touch_point)) {
        // A fractional coordinate was found, but the calculation failed, so
        // drop the event completely. ComputeXY() will already log a failure in
        // this case.
        return;
      }
      touch_point.set_x(new_x);
      touch_point.set_y(new_y);
    }
  }

  InputFilter::InjectTouchEvent(new_event);
}

bool FractionalInputFilter::ComputeXY(int& new_x,
                                      int& new_y,
                                      const auto& event) {
  if (!event.has_fractional_coordinate()) {
    return false;
  }
  const FractionalCoordinate& fractional = event.fractional_coordinate();
  if (!fractional.has_x() || !fractional.has_y()) {
    LOG(ERROR) << "Event has incomplete fractional coordinates.";
    return false;
  }

  int bounds_x;
  int bounds_y;
  int bounds_width;
  int bounds_height;
  if (fractional.has_screen_id()) {
    auto screen_id = fractional.screen_id();
    VLOG(3) << "screen_id = " << screen_id;
    auto it = base::ranges::find_if(video_layout_.video_track(),
                                    [screen_id](const VideoTrackLayout& track) {
                                      return track.has_screen_id() &&
                                             track.screen_id() == screen_id;
                                    });
    if (it == video_layout_.video_track().end()) {
      LOG(ERROR) << "screen_id " << screen_id
                 << " not found in the video layout.";
      return false;
    }

    const VideoTrackLayout& monitor = *it;

    DCHECK(monitor.has_position_x());
    DCHECK(monitor.has_position_y());
    DCHECK(monitor.has_width());
    DCHECK(monitor.has_height());
    bounds_x = monitor.position_x();
    bounds_y = monitor.position_y();
    bounds_width = monitor.width();
    bounds_height = monitor.height();
  } else {
    if (fallback_geometry_.is_empty()) {
      LOG(ERROR)
          << "Fractional coordinates have no screen_id and no fallback is set.";
      return false;
    }

    bounds_x = fallback_geometry_.left();
    bounds_y = fallback_geometry_.top();
    bounds_width = fallback_geometry_.width();
    bounds_height = fallback_geometry_.height();
  }

  new_x = ScaleAndClamp(fractional.x(), bounds_x, bounds_width);
  new_y = ScaleAndClamp(fractional.y(), bounds_y, bounds_height);
  VLOG(3) << "(" << fractional.x() << ", " << fractional.y() << ") -> ("
          << new_x << ", " << new_y << ")";
  return true;
}

}  // namespace remoting::protocol
