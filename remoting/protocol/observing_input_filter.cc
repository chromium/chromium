// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/observing_input_filter.h"

namespace remoting::protocol {

ObservingInputFilter::ObservingInputFilter(InputStub* input_stub)
    : InputFilter(input_stub) {}

ObservingInputFilter::~ObservingInputFilter() = default;

void ObservingInputFilter::InjectKeyEvent(const KeyEvent& event) {
  if (on_input_event_) {
    on_input_event_.Run(event);
  }

  InputFilter::InjectKeyEvent(event);
}

void ObservingInputFilter::InjectTextEvent(const TextEvent& event) {
  if (on_input_event_) {
    on_input_event_.Run(event);
  }

  InputFilter::InjectTextEvent(event);
}

void ObservingInputFilter::InjectMouseEvent(const MouseEvent& event) {
  if (on_input_event_) {
    on_input_event_.Run(event);
  }

  InputFilter::InjectMouseEvent(event);
}

void ObservingInputFilter::InjectTouchEvent(const TouchEvent& event) {
  if (on_input_event_) {
    on_input_event_.Run(event);
  }

  InputFilter::InjectTouchEvent(event);
}

void ObservingInputFilter::SetInputEventCallback(
    InputEventCallback on_input_event) {
  on_input_event_ = std::move(on_input_event);
}

void ObservingInputFilter::ClearInputEventCallback() {
  on_input_event_.Reset();
}

}  // namespace remoting::protocol
