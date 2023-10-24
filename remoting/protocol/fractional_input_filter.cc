// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fractional_input_filter.h"

#include <algorithm>

#include "base/ranges/algorithm.h"
#include "remoting/base/logging.h"
#include "remoting/proto/event.pb.h"

namespace remoting::protocol {

namespace {

// Scales `fraction` (between 0 and 1) to a value between `minimum` and
// (minimum + size - 1).
int ScaleAndClamp(float fraction, int minimum, int size) {
  int scaled = static_cast<int>(fraction * size + 0.5);
  scaled = std::clamp(scaled, 0, size - 1);
  return scaled + minimum;
}

// Attempts to use the event's fractional coordinates to compute new x,y values
// for the event. Returns true if successful and new_x, new_y hold the values.
// The event must be a protobuf type with x, y and fractional_coordinate fields.
template <typename E>
bool ComputeXY(int& new_x,
               int& new_y,
               const E& event,
               const VideoLayout& layout) {
  if (!event.has_fractional_coordinate()) {
    return false;
  }
  const FractionalCoordinate& fractional = event.fractional_coordinate();
  if (!fractional.has_x() || !fractional.has_y() ||
      !fractional.has_screen_id()) {
    LOG(ERROR) << "Event has incomplete fractional coordinates.";
    return false;
  }
  auto screen_id = fractional.screen_id();
  auto it = base::ranges::find_if(
      layout.video_track(), [screen_id](const VideoTrackLayout& track) {
        return track.has_screen_id() && track.screen_id() == screen_id;
      });
  if (it == layout.video_track().end()) {
    LOG(ERROR) << "Fractional coordinate has invalid screen_id " << screen_id;
    return false;
  }
  const VideoTrackLayout& monitor = *it;

  // The VideoLayout comes from DesktopDisplayInfo::GetVideoLayoutProto() so all
  // fields are expected to be present.
  DCHECK(monitor.has_position_x());
  DCHECK(monitor.has_position_y());
  DCHECK(monitor.has_width());
  DCHECK(monitor.has_height());
  new_x = ScaleAndClamp(fractional.x(), monitor.position_x(), monitor.width());
  new_y = ScaleAndClamp(fractional.y(), monitor.position_y(), monitor.height());
  return true;
}

}  // namespace

FractionalInputFilter::FractionalInputFilter() = default;

FractionalInputFilter::FractionalInputFilter(InputStub* input_stub)
    : InputFilter(input_stub) {}

FractionalInputFilter::~FractionalInputFilter() = default;

void FractionalInputFilter::set_video_layout(const VideoLayout& layout) {
  video_layout_ = layout;
}

void FractionalInputFilter::InjectMouseEvent(const MouseEvent& event) {
  if (!event.has_fractional_coordinate()) {
    InputFilter::InjectMouseEvent(event);
    return;
  }

  int new_x;
  int new_y;
  if (ComputeXY(new_x, new_y, event, video_layout_)) {
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
      if (!ComputeXY(new_x, new_y, touch_point, video_layout_)) {
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

}  // namespace remoting::protocol
