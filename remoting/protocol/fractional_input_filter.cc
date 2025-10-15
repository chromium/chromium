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
  // Copy the event, so it can be mutated. This could be optimized for cases
  // where mutation is not needed. But in the longer term, the TouchEvents will
  // all have fractional coordinates, and then a copy is needed anyway.
  TouchEvent new_event(event);

  for (TouchEventPoint& touch_point : *(new_event.mutable_touch_points())) {
    // Events with no fractional-coordinates should be passed through unchanged.
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
