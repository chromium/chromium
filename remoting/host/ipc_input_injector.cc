// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_input_injector.h"

#include <utility>

#include "remoting/host/desktop_session_proxy.h"

namespace remoting {

IpcInputInjector::IpcInputInjector(
    scoped_refptr<DesktopSessionProxy> desktop_session_proxy)
    : desktop_session_proxy_(desktop_session_proxy) {}

IpcInputInjector::~IpcInputInjector() = default;

void IpcInputInjector::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  desktop_session_proxy_->InjectClipboardEvent(event);
}

void IpcInputInjector::InjectKeyEvent(const protocol::KeyEvent& event) {
  desktop_session_proxy_->InjectKeyEvent(event);
}

void IpcInputInjector::InjectTextEvent(const protocol::TextEvent& event) {
  desktop_session_proxy_->InjectTextEvent(event);
}

void IpcInputInjector::InjectMouseEvent(const protocol::MouseEvent& event) {
  desktop_session_proxy_->InjectMouseEvent(event);
}

void IpcInputInjector::InjectTouchEvent(const protocol::TouchEvent& event) {
  desktop_session_proxy_->InjectTouchEvent(event);
}

void IpcInputInjector::Start(
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {
  desktop_session_proxy_->StartInputInjector(std::move(client_clipboard));
}

}  // namespace remoting
