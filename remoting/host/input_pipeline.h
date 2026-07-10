// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INPUT_PIPELINE_H_
#define REMOTING_HOST_INPUT_PIPELINE_H_

#include "base/sequence_checker.h"
#include "remoting/host/cursor_visibility_notifier.h"
#include "remoting/host/remote_input_filter.h"
#include "remoting/protocol/fractional_input_filter.h"
#include "remoting/protocol/input_event_tracker.h"
#include "remoting/protocol/input_filter.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/observing_input_filter.h"

namespace remoting {

namespace protocol {
class CoordinateConverter;
}  // namespace protocol

// InputPipeline encapsulates the chain of input filters used to process and
// validate input events before they are injected into the host session.
class InputPipeline : public protocol::InputStub {
 public:
  InputPipeline(
      protocol::CoordinateConverter* coordinate_converter,
      CursorVisibilityNotifier::EventHandler* cursor_visibility_event_handler);

  InputPipeline(const InputPipeline&) = delete;
  InputPipeline& operator=(const InputPipeline&) = delete;

  ~InputPipeline() override;

  // protocol::InputStub interface.
  void InjectKeyEvent(const protocol::KeyEvent& event) override;
  void InjectTextEvent(const protocol::TextEvent& event) override;
  void InjectMouseEvent(const protocol::MouseEvent& event) override;
  void InjectTouchEvent(const protocol::TouchEvent& event) override;

  // Sets the downstream InputStub to which this pipeline will forward events
  // (e.g. the InputInjector).
  void SetInputStub(protocol::InputStub* input_stub);

  // Accessors for configuring or observing specific filters in the chain.
  protocol::InputFilter* disable_input_filter() {
    return &disable_input_filter_;
  }
  protocol::ObservingInputFilter* observing_input_filter() {
    return &observing_input_filter_;
  }
  RemoteInputFilter* remote_input_filter() { return &remote_input_filter_; }
  CursorVisibilityNotifier* cursor_visibility_notifier() {
    return &cursor_visibility_notifier_;
  }
  protocol::InputEventTracker* input_tracker() { return &input_tracker_; }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Upstream filters hold raw pointers to downstream ones.
  // To ensure safe destruction, downstream filters must be declared first
  // so they are destroyed last.
  //
  // Event flow goes in reverse of declaration order.

  // Tracker used to release pressed keys and buttons when disconnecting.
  protocol::InputEventTracker input_tracker_;

  // Filter used to detect transitions into and out of client-side pointer lock,
  // and to monitor local input to determine whether or not to include the mouse
  // cursor in the desktop image.
  CursorVisibilityNotifier cursor_visibility_notifier_;

  // Filter used to disable remote inputs during local input activity.
  RemoteInputFilter remote_input_filter_;

  // Filter used to convert any fractional coordinates to input-injection
  // coordinates.
  protocol::FractionalInputFilter fractional_input_filter_;

  // Filter used to notify listeners when remote input events are received.
  protocol::ObservingInputFilter observing_input_filter_;

  // Filters used to manage enabling & disabling of input.
  protocol::InputFilter disable_input_filter_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_INPUT_PIPELINE_H_
