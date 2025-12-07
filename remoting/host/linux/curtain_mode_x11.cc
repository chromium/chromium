// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "remoting/base/logging.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/curtain_mode.h"
#include "remoting/host/linux/x11_util.h"

namespace remoting {

class CurtainModeX11 : public CurtainMode {
 public:
  CurtainModeX11();

  CurtainModeX11(const CurtainModeX11&) = delete;
  CurtainModeX11& operator=(const CurtainModeX11&) = delete;

  // Overriden from CurtainMode.
  bool Activate() override;
};

CurtainModeX11::CurtainModeX11() = default;

bool CurtainModeX11::Activate() {
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
  return std::make_unique<CurtainModeX11>();
}

}  // namespace remoting
