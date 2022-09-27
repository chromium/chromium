// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/curtain_mode_chromeos.h"

#include <memory>

#include "ash/curtain/security_curtain_controller.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/chromeos/ash_proxy.h"

namespace remoting {

// static
std::unique_ptr<CurtainMode> CurtainMode::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control) {
  return std::make_unique<CurtainModeChromeOs>(ui_task_runner);
}

CurtainModeChromeOs::CurtainModeChromeOs(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : core_(ui_task_runner) {}

CurtainModeChromeOs::~CurtainModeChromeOs() = default;

bool CurtainModeChromeOs::Activate() {
  core_.AsyncCall(&Core::Activate);
  return true;
}

CurtainModeChromeOs::Core::~Core() {
  security_curtain_controller().Disable();
}

void CurtainModeChromeOs::Core::Activate() {
  security_curtain_controller().Enable();
}

ash::curtain::SecurityCurtainController&
CurtainModeChromeOs::Core::security_curtain_controller() {
  return AshProxy::Get().GetSecurityCurtainController();
}

}  // namespace remoting
