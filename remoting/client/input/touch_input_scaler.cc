// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/input/touch_input_scaler.h"

#include <algorithm>

#include "base/check_op.h"
#include "remoting/proto/event.pb.h"

namespace remoting {

using protocol::TouchEvent;
using protocol::TouchEventPoint;

namespace {

// |value| is the number to be scaled. |output_max| is the output desktop's max
// height or width. |input_max| is the input desktop's max height or width.
float Scale(float value, int output_max, int input_max) {
  DCHECK_GT(output_max, 0);
  DCHECK_GT(input_max, 0);
  value *= output_max;
  value /= input_max;
  return value;
}

// Same as Scale() but |value| will be scaled and clamped using |output_max| and
// |input_max|.
float ScaleAndClamp(float value, int output_max, int input_max) {
  value = Scale(value, output_max, input_max);
  return std::clamp(value, 0.0f, static_cast<float>(output_max));
}

}  // namespace

TouchInputScaler::TouchInputScaler(InputStub* input_stub)
    : InputFilter(input_stub) {}

TouchInputScaler::~TouchInputScaler() = default;

void TouchInputScaler::InjectTouchEvent(const TouchEvent& event) {
  if (input_size_.is_empty() || output_size_.is_empty())
    return;

  // We scale based on the maximum input & output coordinates, rather than the
  // input and output sizes, so that it's possible to reach the edge of the
  // output when up-scaling.  We also take care to round up or down correctly,
  // which is important when down-scaling.
  TouchEvent out_event(event);
  for (int i = 0; i < out_event.touch_points().size(); ++i) {
    TouchEventPoint* point = out_event.mutable_touch_points(i);
    if (point->has_x() || point->has_y()) {
      DCHECK(point->has_x() && point->has_y());
      point->set_x(
          ScaleAndClamp(point->x(), output_size_.width(), input_size_.width()));
      point->set_y(ScaleAndClamp(point->y(), output_size_.height(),
                                 input_size_.height()));
    }

    // Also scale the touch size. Without scaling, the size on the host will not
    // be right.
    // For example
    // Suppose:
    //  - No size scaling.
    //  - Client is a HiDPI Chromebook device.
    //  - Host is running on a HiDPI Windows device.
    // With the configuration above, the client will send the logical touch
    // size to the host, therefore it will be smaller on the host.
    // This is because a HiDPI Chromebook device (e.g. Pixel) has 2 by 2
    // physical pixel mapped to a logical pixel.
    // With scaling, the size would be the same.
    // Note that there's no need to clamp the touch point size. For example on
    // a Nexus4 device, part of the touch circle falls outside the screen on
    // edges but still functions correctly.
    if (point->has_radius_x() || point->has_radius_y()) {
      DCHECK(point->has_radius_x() && point->has_radius_y());
      point->set_radius_x(
          Scale(point->radius_x(), output_size_.width(), input_size_.width()));
      point->set_radius_y(Scale(point->radius_y(), output_size_.height(),
                                input_size_.height()));
    }
  }

  InputFilter::InjectTouchEvent(out_event);
}

}  // namespace remoting
