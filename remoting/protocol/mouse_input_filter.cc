// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/mouse_input_filter.h"

#include <algorithm>

#include "base/logging.h"
#include "remoting/base/logging.h"
#include "remoting/proto/event.pb.h"

namespace remoting::protocol {

MouseInputFilter::MouseInputFilter() = default;

MouseInputFilter::MouseInputFilter(InputStub* input_stub)
    : InputFilter(input_stub) {}

MouseInputFilter::~MouseInputFilter() = default;

void MouseInputFilter::InjectMouseEvent(const MouseEvent& event) {
  // Pass unchanged any event which has fractional-coordinates. These events
  // will be handled by FractionalInputFilter.
  if (event.has_fractional_coordinate()) {
    InputFilter::InjectMouseEvent(event);
    return;
  }

  if (input_bounds_.is_zero()) {
    HOST_LOG << "Dropping mouse event because input bounds are unset";
    return;
  }
  if (output_bounds_.is_zero()) {
    HOST_LOG << "Dropping mouse event because output bounds are unset";
    return;
  }

  if (event.has_x() && event.x() > input_bounds_.x() && event.has_y() &&
      event.y() > input_bounds_.y()) {
    // Mouse events are scaled by the client to what it believes the desktop
    // size to be, so receiving an event outside this rect should be rare.
    HOST_LOG << "Mouse event (" << event.x() << "," << event.y()
             << ") is outside input rect " << input_bounds_.x() << "x"
             << input_bounds_.y();
  }

  // We scale based on the max input and output coordinates (which are equal
  // to size-1), rather than the input and output sizes, so that it's possible
  // to reach the edge of the output when up-scaling.  We also take care to
  // round up or down correctly, which is important when down-scaling.
  // After scaling, we offset by the output rect origin. This is normally
  // (0,0), but may be non-zero when there are multiple displays and we are
  // showing a single display.

  MouseEvent out_event(event);
  if (out_event.has_x()) {
    out_event.set_x(output_offset_.x() + GetScaledX(out_event.x()));
  }
  if (out_event.has_y()) {
    out_event.set_y(output_offset_.y() + GetScaledY(out_event.y()));
  }
  InputFilter::InjectMouseEvent(out_event);
}

void MouseInputFilter::set_input_size(const int32_t x, const int32_t y) {
  input_bounds_ = webrtc::DesktopVector(std::max(x - 1, 0), std::max(y - 1, 0));
  HOST_LOG << "Setting MouseInputFilter input boundary to " << input_bounds_.x()
           << "," << input_bounds_.y();
}

void MouseInputFilter::set_output_size(const int32_t x, const int32_t y) {
  output_bounds_ =
      webrtc::DesktopVector(std::max(x - 1, 0), std::max(y - 1, 0));
  HOST_LOG << "Setting MouseInputFilter output boundary to "
           << output_bounds_.x() << "," << output_bounds_.y();
}

void MouseInputFilter::set_output_offset(const webrtc::DesktopVector& v) {
  output_offset_ = webrtc::DesktopVector(v.x(), v.y());
  HOST_LOG << "Setting MouseInputFilter output_offset to " << output_offset_.x()
           << "," << output_offset_.y();
}

int32_t MouseInputFilter::GetScaledX(int32_t x) {
  if (output_bounds_.x() != input_bounds_.x()) {
    x = ((x * output_bounds_.x()) + (input_bounds_.x() / 2)) /
        input_bounds_.x();
  }
  return std::clamp(x, 0, output_bounds_.x());
}

int32_t MouseInputFilter::GetScaledY(int32_t y) {
  if (output_bounds_.y() != input_bounds_.y()) {
    y = ((y * output_bounds_.y()) + (input_bounds_.y() / 2)) /
        input_bounds_.y();
  }
  return std::clamp(y, 0, output_bounds_.y());
}

}  // namespace remoting::protocol
