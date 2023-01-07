// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_and_cursor_composer_notifier.h"

#include "remoting/proto/event.pb.h"

namespace remoting {
DesktopAndCursorComposerNotifier::DesktopAndCursorComposerNotifier(
    InputStub* input_stub,
    EventHandler* event_handler)
    : InputFilter(input_stub), event_handler_(event_handler) {}

DesktopAndCursorComposerNotifier::~DesktopAndCursorComposerNotifier() = default;

void DesktopAndCursorComposerNotifier::InjectMouseEvent(
    const protocol::MouseEvent& event) {
  NotifyEventHandler(event.has_delta_x() || event.has_delta_y());

  InputFilter::InjectMouseEvent(event);
}

void DesktopAndCursorComposerNotifier::OnLocalInput() {
  NotifyEventHandler(true);
}

void DesktopAndCursorComposerNotifier::NotifyEventHandler(bool enabled) {
  if (enabled != is_enabled_ || !has_triggered_) {
    event_handler_->SetComposeEnabled(enabled);
    is_enabled_ = enabled;
    has_triggered_ = true;
  }
}

}  // namespace remoting
