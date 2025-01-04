// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/curtain_mode.h"

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "remoting/base/logging.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/linux/wayland_utils.h"
#include "remoting/host/linux/x11_util.h"

namespace remoting {

class CurtainModeLinux : public CurtainMode {
 public:
  CurtainModeLinux();

  CurtainModeLinux(const CurtainModeLinux&) = delete;
  CurtainModeLinux& operator=(const CurtainModeLinux&) = delete;

  // Overriden from CurtainMode.
  bool Activate() override;
};

CurtainModeLinux::CurtainModeLinux() = default;

bool CurtainModeLinux::Activate() {
  if (IsRunningWayland()) {
    // Our wayland implementation runs headlessly on a session with a previously
    // unused / new display socket, so we can assume that the session is
    // curtained.
    return true;
  }

  // We can't curtain the session in run-time in Linux.
  // Either the session is running in a virtual session (i.e. always curtained),
  // or it is attached to the physical console (i.e. impossible to curtain).
  x11::Connection* connection = x11::Connection::Get();
  if (IsVirtualSession(connection)) {
    return true;
  } else {
    LOG(ERROR) << "Curtain-mode is not supported when running on non-virtual "
                  "X server";
    return false;
  }
}

// static
std::unique_ptr<CurtainMode> CurtainMode::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control) {
  return std::make_unique<CurtainModeLinux>();
}

}  // namespace remoting
