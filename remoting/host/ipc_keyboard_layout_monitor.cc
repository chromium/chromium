// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_keyboard_layout_monitor.h"

#include <utility>

#include "remoting/host/desktop_session_proxy.h"

namespace remoting {

IpcKeyboardLayoutMonitor::IpcKeyboardLayoutMonitor(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback,
    scoped_refptr<DesktopSessionProxy> desktop_session_proxy)
    : callback_(std::move(callback)),
      desktop_session_proxy_(std::move(desktop_session_proxy)),
      weak_ptr_factory_(this) {}

IpcKeyboardLayoutMonitor::~IpcKeyboardLayoutMonitor() = default;

void IpcKeyboardLayoutMonitor::Start() {
  desktop_session_proxy_->SetKeyboardLayoutMonitor(
      weak_ptr_factory_.GetWeakPtr());
  if (desktop_session_proxy_->GetKeyboardCurrentLayout()) {
    callback_.Run(*desktop_session_proxy_->GetKeyboardCurrentLayout());
  }
}

void remoting::IpcKeyboardLayoutMonitor::OnKeyboardChanged(
    const protocol::KeyboardLayout& layout) {
  callback_.Run(layout);
}

}  // namespace remoting
