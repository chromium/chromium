// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/mouse_input_filter.h"

#include <algorithm>

#include "base/cxx17_backports.h"
#include "base/logging.h"
#include "remoting/proto/event.pb.h"

namespace remoting {
namespace protocol {

MouseInputFilter::MouseInputFilter()
    : x_input_(0), y_input_(0), x_output_(0), y_output_(0) {}

MouseInputFilter::MouseInputFilter(InputStub* input_stub)
    : InputFilter(input_stub),
      x_input_(0),
      y_input_(0),
      x_output_(0),
      y_output_(0) {}

MouseInputFilter::~MouseInputFilter() = default;

void MouseInputFilter::InjectMouseEvent(const MouseEvent& event) {
  if (x_input_ == 0 || y_input_ == 0 || x_output_ == 0 || y_output_ == 0) {
    return;
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
    int x = out_event.x();
    if (x_output_ != x_input_)
      x = ((x * x_output_) + (x_input_ / 2)) / x_input_;
    out_event.set_x(output_offset_.x() + base::clamp(x, 0, x_output_));
  }
  if (out_event.has_y()) {
    int y = out_event.y();
    if (y_output_ != y_input_)
      y = ((y * y_output_) + (y_input_ / 2)) / y_input_;
    out_event.set_y(output_offset_.y() + base::clamp(y, 0, y_output_));
  }
  InputFilter::InjectMouseEvent(out_event);
}

void MouseInputFilter::set_input_size(const int32_t x, const int32_t y) {
  x_input_ = x - 1;
  y_input_ = y - 1;
  LOG(INFO) << "Setting MouseInputFilter input_size to " << x_input_ << ","
            << y_input_;
}

void MouseInputFilter::set_output_size(const int32_t x, const int32_t y) {
  x_output_ = x - 1;
  y_output_ = y - 1;
  LOG(INFO) << "Setting MouseInputFilter output_size to " << x_output_ << ","
            << y_output_;
}

void MouseInputFilter::set_output_offset(const webrtc::DesktopVector& v) {
  output_offset_ = webrtc::DesktopVector(v.x(), v.y());
  LOG(INFO) << "Setting MouseInputFilter output_offset to "
            << output_offset_.x() << "," << output_offset_.y();
}

}  // namespace protocol
}  // namespace remoting
