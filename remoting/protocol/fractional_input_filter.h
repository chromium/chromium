// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FRACTIONAL_INPUT_FILTER_H_
#define REMOTING_PROTOCOL_FRACTIONAL_INPUT_FILTER_H_

#include "remoting/proto/control.pb.h"
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
  FractionalInputFilter();
  explicit FractionalInputFilter(InputStub* input_stub);

  FractionalInputFilter(const FractionalInputFilter&) = delete;
  FractionalInputFilter& operator=(const FractionalInputFilter&) = delete;

  ~FractionalInputFilter() override;

  // Sets the video layout to be used to convert fractional coordinates for
  // injection.
  void set_video_layout(const VideoLayout& layout);

  // This method sets the fallback geometry to be used for fractional
  // coordinates which don't have `screen_id`. If no fallback is set (or has
  // empty size), no fallback will be used - events will be dropped if
  // their fractional coordinates don't include any screen_id.
  void set_fallback_geometry(webrtc::DesktopRect geometry);

  // InputStub overrides.
  void InjectMouseEvent(const MouseEvent& event) override;
  void InjectTouchEvent(const TouchEvent& event) override;

 private:
  // Attempts to use the event's fractional coordinates to compute new x,y
  // values for the event. Returns true if successful and new_x, new_y hold the
  // values. The event must be a protobuf type with x, y and
  // fractional_coordinate fields.
  bool ComputeXY(int& new_x, int& new_y, const auto& event);

  VideoLayout video_layout_;

  // webrtc::DesktopRect is a convenient choice because it uses 32-bit values
  // which match the proto definitions for VideoTrackLayout fields.
  webrtc::DesktopRect fallback_geometry_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_FRACTIONAL_INPUT_FILTER_H_
