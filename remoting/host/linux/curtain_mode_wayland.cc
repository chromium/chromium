// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/curtain_mode.h"

#include <memory>

#include "base/callback.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"

namespace remoting {

namespace {
class CurtainModeWayland : public CurtainMode {
 public:
  CurtainModeWayland();

  CurtainModeWayland(const CurtainModeWayland&) = delete;
  CurtainModeWayland& operator=(const CurtainModeWayland&) = delete;

  // Overriden from CurtainMode.
  bool Activate() override;
};

CurtainModeWayland::CurtainModeWayland() = default;

bool CurtainModeWayland::Activate() {
  // Our implementation runs headlessly on a session with a previously
  // unused / new display socket, so we can assume that the session is
  // curtained.
  return true;
}

}  // namespace

// static
std::unique_ptr<CurtainMode> CurtainMode::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control) {
  return std::make_unique<CurtainModeWayland>();
}

}  // namespace remoting
