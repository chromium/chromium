// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_pipeline.h"

#include "base/functional/bind.h"
#include "remoting/protocol/coordinate_converter.h"

namespace remoting {

InputPipeline::InputPipeline(
    protocol::CoordinateConverter* coordinate_converter,
    CursorVisibilityNotifier::EventHandler* cursor_visibility_event_handler)
    : cursor_visibility_notifier_(&input_tracker_,
                                  cursor_visibility_event_handler),
      remote_input_filter_(
          &cursor_visibility_notifier_,
          // Unretained() is safe because `remote_input_filter_` will be
          // destroyed before `input_tracker_`, after which the callback will no
          // longer be called.
          base::BindRepeating(&protocol::InputEventTracker::ReleaseAll,
                              base::Unretained(&input_tracker_))),
      fractional_input_filter_(&remote_input_filter_, coordinate_converter),
      observing_input_filter_(&fractional_input_filter_),
      disable_input_filter_(&observing_input_filter_) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

InputPipeline::~InputPipeline() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void InputPipeline::InjectKeyEvent(const protocol::KeyEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  disable_input_filter_.InjectKeyEvent(event);
}

void InputPipeline::InjectTextEvent(const protocol::TextEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  disable_input_filter_.InjectTextEvent(event);
}

void InputPipeline::InjectMouseEvent(const protocol::MouseEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  disable_input_filter_.InjectMouseEvent(event);
}

void InputPipeline::InjectTouchEvent(const protocol::TouchEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  disable_input_filter_.InjectTouchEvent(event);
}

void InputPipeline::SetInputStub(protocol::InputStub* input_stub) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  input_tracker_.set_input_stub(input_stub);
}

}  // namespace remoting
