// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fractional_input_filter.h"

#include <algorithm>

#include "base/check.h"
#include "remoting/proto/event.pb.h"

namespace remoting::protocol {

FractionalInputFilter::FractionalInputFilter(
    InputStub* input_stub,
    const CoordinateConverter* converter)
    : InputFilter(input_stub), converter_(converter) {
  DCHECK(converter_);
}

FractionalInputFilter::~FractionalInputFilter() = default;

void FractionalInputFilter::InjectMouseEvent(const MouseEvent& event) {
  if (!event.has_fractional_coordinate()) {
    if (event.has_x() || event.has_y()) {
      // Drop absolute mouse events that lack fractional coordinates.
      // In multi-stream mode, absolute positioning requires a screen_id to
      // scale and clamp to the correct display. Legacy absolute coordinates
      // lack a screen_id, and there is no fallback geometry to clamp against.
      return;
    }
    InputFilter::InjectMouseEvent(event);
    return;
  }

  auto result =
      converter_->ToGlobalAbsoluteCoordinate(event.fractional_coordinate());
  if (result) {
    MouseEvent new_event(event);
    new_event.set_x(result->x());
    new_event.set_y(result->y());
    InputFilter::InjectMouseEvent(new_event);
  }
}

void FractionalInputFilter::InjectTouchEvent(const TouchEvent& event) {
  for (const TouchEventPoint& touch_point : event.touch_points()) {
    if (!touch_point.has_fractional_coordinate()) {
      if (touch_point.has_x() || touch_point.has_y()) {
        // Drop absolute touch events that lack fractional coordinates, as
        // they lack a screen_id and cannot be safely scaled or clamped.
        return;
      }
    }
  }

  // Copy the event, so it can be mutated. This could be optimized for cases
  // where mutation is not needed. But in the longer term, the TouchEvents will
  // all have fractional coordinates, and then a copy is needed anyway.
  TouchEvent new_event(event);

  for (TouchEventPoint& touch_point : *(new_event.mutable_touch_points())) {
    if (touch_point.has_fractional_coordinate()) {
      auto result = converter_->ToGlobalAbsoluteCoordinate(
          touch_point.fractional_coordinate());
      if (!result) {
        // A fractional coordinate was found, but the calculation failed, so
        // drop the event completely. ComputeXY() will already log a failure in
        // this case.
        return;
      }
      touch_point.set_x(result->x());
      touch_point.set_y(result->y());
    }
  }

  InputFilter::InjectTouchEvent(new_event);
}

}  // namespace remoting::protocol
