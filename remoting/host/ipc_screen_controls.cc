// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_screen_controls.h"

#include "base/notreached.h"
#include "remoting/host/desktop_session_proxy.h"

namespace remoting {

IpcScreenControls::IpcScreenControls(
    scoped_refptr<DesktopSessionProxy> desktop_session_proxy)
    : desktop_session_proxy_(desktop_session_proxy) {}

IpcScreenControls::~IpcScreenControls() = default;

void IpcScreenControls::SetScreenResolution(
    const ScreenResolution& resolution,
    std::optional<webrtc::ScreenId> screen_id) {
  // TODO(crbug.com/40225767): Pass |screen_id| over IPC.
  desktop_session_proxy_->SetScreenResolution(resolution);
}

void IpcScreenControls::SetVideoLayout(
    const protocol::VideoLayout& video_layout) {
  NOTIMPLEMENTED();
}

}  // namespace remoting
