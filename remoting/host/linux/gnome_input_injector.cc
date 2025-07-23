// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_input_injector.h"

#include "base/notimplemented.h"
#include "remoting/host/linux/gnome_interaction_strategy.h"

namespace remoting {

GnomeInputInjector::GnomeInputInjector(
    base::WeakPtr<GnomeInteractionStrategy> session)
    : session_(std::move(session)) {}

GnomeInputInjector::~GnomeInputInjector() = default;

void GnomeInputInjector::Start(
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {}

void GnomeInputInjector::InjectKeyEvent(const protocol::KeyEvent& event) {
  if (!session_) {
    return;
  }
  session_->InjectKeyEvent(event);
}

void GnomeInputInjector::InjectTextEvent(const protocol::TextEvent& event) {
  NOTIMPLEMENTED();
}

void GnomeInputInjector::InjectMouseEvent(const protocol::MouseEvent& event) {
  if (!session_) {
    return;
  }
  session_->InjectMouseEvent(event);
}

void GnomeInputInjector::InjectTouchEvent(const protocol::TouchEvent& event) {
  NOTIMPLEMENTED();
}

void GnomeInputInjector::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  NOTIMPLEMENTED();
}

}  // namespace remoting
