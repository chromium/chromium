// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_INPUT_TOUCH_INPUT_SCALER_H_
#define REMOTING_CLIENT_INPUT_TOUCH_INPUT_SCALER_H_

#include "remoting/protocol/input_filter.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

namespace protocol {
class TouchEvent;
}  // namespace protocol

// Scales the touch input coordinates to host coordinates and clamps so
// that the values do not go outside the remote desktop.
// Also resizes the touch size.
class TouchInputScaler : public protocol::InputFilter {
 public:
  TouchInputScaler() = default;
  explicit TouchInputScaler(InputStub* input_stub);

  TouchInputScaler(const TouchInputScaler&) = delete;
  TouchInputScaler& operator=(const TouchInputScaler&) = delete;

  ~TouchInputScaler() override;

  // Set input and output desktop sizes.
  void set_input_size(const webrtc::DesktopSize& size) {
    input_size_.set(size.width() - 1, size.height() - 1);
  }
  void set_output_size(const webrtc::DesktopSize& size) {
    output_size_.set(size.width() - 1, size.height() - 1);
  }

  // protocol::InputStub interface.
  void InjectTouchEvent(const protocol::TouchEvent& event) override;

 private:
  // Sizes for scaling and clamping coordinates and sizes.
  // These hold the max-X,Y coordinates and not the actual width and height.
  webrtc::DesktopSize input_size_;
  webrtc::DesktopSize output_size_;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_INPUT_TOUCH_INPUT_SCALER_H_
