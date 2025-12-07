// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FRACTIONAL_INPUT_FILTER_H_
#define REMOTING_PROTOCOL_FRACTIONAL_INPUT_FILTER_H_

#include "base/memory/raw_ptr.h"
#include "remoting/proto/control.pb.h"
#include "remoting/protocol/coordinate_converter.h"
#include "remoting/protocol/input_filter.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting::protocol {

// Filter which modifies each event's x,y values based on any fractional
// coordinates in the event. Events without fractional coordinates are
// passed through unmodified. The computed x,y values are suitable to be passed
// directly to the InputInjector.
//
// The fractional coordinates are expected to lie between 0 and 1. The
// calculated values are clamped to the monitor's area, to guard against
// unexpected behavior for out-of-range inputs.
//
// The client may send fractional coordinates without a `screen_id` property. In
// this case, the fallback geometry will be used for the scaling calculation.
//
// For MouseEvents with fractional coordinates, if the calculation fails, the
// event is discarded (as it is likely a broken or outdated event which should
// not be injected). Reasons for failure include:
// * A fractional-coordinate field (x or y) is not present.
// * The screen_id is present but is not found in the latest video-layout.
// * The screen_id is not present, and no fallback geometry has been set.
//
// For TouchEvents, these can have any number of TouchEventPoints. If any
// touch-point has a fractional-coordinate but the calculation fails,
// the whole TouchEvent is discarded.
//
// This filter may appear before or after MouseInputFilter in the input
// pipeline, since their actions are mutually-exclusive - MouseInputFilter
// passes through any event with fractional coordinates.
//
// Because this filter may modify the event's x,y values, it may change their
// meaning (from client-provided to host-calculated). Therefore, any other
// filter that cares about x,y values needs to be aware of this transformation.
class FractionalInputFilter : public InputFilter {
 public:
  // Both `input_stub` and `converter` must outlive `this`.
  FractionalInputFilter(InputStub* input_stub,
                        const CoordinateConverter* converter);

  FractionalInputFilter(const FractionalInputFilter&) = delete;
  FractionalInputFilter& operator=(const FractionalInputFilter&) = delete;

  ~FractionalInputFilter() override;

  // InputStub overrides.
  void InjectMouseEvent(const MouseEvent& event) override;
  void InjectTouchEvent(const TouchEvent& event) override;

 private:
  raw_ptr<const CoordinateConverter> converter_;

  VideoLayout video_layout_;

  // webrtc::DesktopRect is a convenient choice because it uses 32-bit values
  // which match the proto definitions for VideoTrackLayout fields.
  webrtc::DesktopRect fallback_geometry_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_FRACTIONAL_INPUT_FILTER_H_
