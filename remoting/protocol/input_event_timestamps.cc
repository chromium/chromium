// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/input_event_timestamps.h"

namespace remoting::protocol {

InputEventTimestampsSourceImpl::InputEventTimestampsSourceImpl() = default;
InputEventTimestampsSourceImpl::~InputEventTimestampsSourceImpl() = default;

void InputEventTimestampsSourceImpl::OnEventReceived(
    InputEventTimestamps timestamps) {
  last_timestamps_ = timestamps;
}

InputEventTimestamps InputEventTimestampsSourceImpl::TakeLastEventTimestamps() {
  InputEventTimestamps result = last_timestamps_;
  last_timestamps_ = InputEventTimestamps();
  return result;
}

}  // namespace remoting::protocol
