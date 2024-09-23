// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_input_filter.h"

#include <stdint.h>

#include "base/logging.h"
#include "remoting/proto/event.pb.h"

namespace {

// The number of remote mouse events to record for the purpose of eliminating
// "echoes" detected by the local input detector. The value should be large
// enough to cope with the fact that multiple events might be injected before
// any echoes are detected.
const unsigned int kNumRemoteMousePositions = 50;

// The number of remote keypress events to record for the purpose of eliminating
// "echoes" detected by the local input detector. The value should be large
// enough to cope with the fact that multiple events might be injected before
// any echoes are detected.
const unsigned int kNumRemoteKeyPresses = 20;

// The number of milliseconds for which to block remote input when local input
// is received.
const int64_t kRemoteBlockTimeoutMillis = 2000;

}  // namespace

namespace remoting {

RemoteInputFilter::RemoteInputFilter(protocol::InputEventTracker* event_tracker)
    : event_tracker_(event_tracker), expect_local_echo_(true) {}

RemoteInputFilter::~RemoteInputFilter() = default;

bool RemoteInputFilter::LocalPointerMoved(const webrtc::DesktopVector& pos,
                                          ui::EventType type) {
  // If this is a genuine local input event (rather than an echo of a remote
  // input event that we've just injected), then ignore remote inputs for a
  // short time.
  //
  // Note that no platforms both inject and monitor for touch events, so echo
  // suppression is only applied to mouse input.
  if (expect_local_echo_ && type == ui::EventType::kMouseMoved) {
    auto found_position = injected_mouse_positions_.begin();
    while (found_position != injected_mouse_positions_.end() &&
           !pos.equals(*found_position)) {
      ++found_position;
    }
    if (found_position != injected_mouse_positions_.end()) {
      // Remove it from the list, and any positions that were added before it,
      // if any.  This is because the local input monitor is assumed to receive
      // injected mouse position events in the order in which they were injected
      // (if at all).  If the position is found somewhere other than the front
      // of the queue, this would be because the earlier positions weren't
      // successfully injected (or the local input monitor might have skipped
      // over some positions), and not because the events were out-of-sequence.
      // These spurious positions should therefore be discarded.
      injected_mouse_positions_.erase(injected_mouse_positions_.begin(),
                                      ++found_position);
      return false;
    }
  }

  LocalInputDetected();
  return true;
}

bool RemoteInputFilter::LocalKeyPressed(uint32_t usb_keycode) {
  // If local echo is expected and |usb_keycode| is the oldest unechoed injected
  // keypress, then ignore it.
  if (expect_local_echo_ && !injected_key_presses_.empty() &&
      injected_key_presses_.front() == usb_keycode) {
    injected_key_presses_.pop_front();
    return false;
  }

  LocalInputDetected();
  return true;
}

void RemoteInputFilter::LocalInputDetected() {
  event_tracker_->ReleaseAll();
  latest_local_input_time_ = base::TimeTicks::Now();
}

void RemoteInputFilter::SetExpectLocalEcho(bool expect_local_echo) {
  expect_local_echo_ = expect_local_echo;
  if (!expect_local_echo_) {
    injected_mouse_positions_.clear();
  }
}

void RemoteInputFilter::InjectKeyEvent(const protocol::KeyEvent& event) {
  if (ShouldIgnoreInput()) {
    return;
  }
  if (expect_local_echo_ && event.pressed() && event.has_usb_keycode()) {
    injected_key_presses_.push_back(event.usb_keycode());
    if (injected_key_presses_.size() > kNumRemoteKeyPresses) {
      VLOG(1) << "Injected key press queue full.";
      injected_key_presses_.clear();
    }
  }
  event_tracker_->InjectKeyEvent(event);
}

void RemoteInputFilter::InjectTextEvent(const protocol::TextEvent& event) {
  if (ShouldIgnoreInput()) {
    return;
  }
  event_tracker_->InjectTextEvent(event);
}

void RemoteInputFilter::InjectMouseEvent(const protocol::MouseEvent& event) {
  if (ShouldIgnoreInput()) {
    return;
  }
  if (expect_local_echo_ && event.has_x() && event.has_y()) {
    injected_mouse_positions_.push_back(
        webrtc::DesktopVector(event.x(), event.y()));
    if (injected_mouse_positions_.size() > kNumRemoteMousePositions) {
      VLOG(1) << "Injected mouse positions queue full.";
      injected_mouse_positions_.pop_front();
    }
  }
  event_tracker_->InjectMouseEvent(event);
}

void RemoteInputFilter::InjectTouchEvent(const protocol::TouchEvent& event) {
  if (ShouldIgnoreInput()) {
    return;
  }
  event_tracker_->InjectTouchEvent(event);
}

bool RemoteInputFilter::ShouldIgnoreInput() const {
  // Ignore remote events if the local mouse moved recently.
  int64_t millis =
      (base::TimeTicks::Now() - latest_local_input_time_).InMilliseconds();
  if (millis < kRemoteBlockTimeoutMillis) {
    return true;
  }
  return false;
}

}  // namespace remoting
