// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_screen_controls.h"

#include "remoting/host/desktop_session_proxy.h"

namespace remoting {

IpcScreenControls::IpcScreenControls(
    scoped_refptr<DesktopSessionProxy> desktop_session_proxy)
    : desktop_session_proxy_(desktop_session_proxy) {
}

IpcScreenControls::~IpcScreenControls() = default;

void IpcScreenControls::SetScreenResolution(
    const ScreenResolution& resolution) {
  desktop_session_proxy_->SetScreenResolution(resolution);
}

}  // namespace remoting
