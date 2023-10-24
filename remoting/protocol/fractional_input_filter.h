// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FRACTIONAL_INPUT_FILTER_H_
#define REMOTING_PROTOCOL_FRACTIONAL_INPUT_FILTER_H_

#include "remoting/proto/control.pb.h"
#include "remoting/protocol/input_filter.h"

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
// For MouseEvents with fractional coordinates, if the calculation fails, the
// event is discarded (as it is likely a broken or outdated event which should
// not be injected). Reasons for failure include:
// * A fractional-coordinate field (x, y, or screen_id) is not present.
// * The screen_id is not found in the latest video-layout.
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

  // InputStub overrides.
  void InjectMouseEvent(const MouseEvent& event) override;
  void InjectTouchEvent(const TouchEvent& event) override;

 private:
  VideoLayout video_layout_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_FRACTIONAL_INPUT_FILTER_H_
