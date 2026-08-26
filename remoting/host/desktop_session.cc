// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_session.h"

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/constants.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/daemon_process.h"

namespace remoting {

DesktopSession::~DesktopSession() = default;

DesktopSession::DesktopSession(DaemonProcess* daemon_process, int id)
    : daemon_process_(daemon_process), id_(id) {}

void DesktopSession::SetReceiver(
    mojo::PendingReceiver<mojom::DesktopSession> receiver) {
  if (receiver.is_valid()) {
    receiver_.reset();
    receiver_.Bind(std::move(receiver));
#if !BUILDFLAG(IS_LINUX)
    // On platforms without persistent desktop sessions, immediately close the
    // desktop session when the control pipe drops so that background agents
    // (e.g. `remoting_desktop.exe`) are torn down and resources released.
    receiver_.set_disconnect_handler(base::BindOnce(
        &DesktopSession::CloseDesktopSession, base::Unretained(this)));
#endif
  }
}

void DesktopSession::SetEventsRemote(
    mojo::PendingRemote<mojom::DesktopSessionEvents> remote) {
  if (remote.is_valid()) {
    events_remote_.reset();
    events_remote_.Bind(std::move(remote));
  }
}

void DesktopSession::CloseDesktopSession() {
  daemon_process_->CloseDesktopSession(id_);
}

}  // namespace remoting
