// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/curtain_mode_chromeos.h"

#include <memory>

#include "ash/curtain/remote_maintenance_curtain_view.h"
#include "ash/curtain/security_curtain_controller.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/chromeos/ash_proxy.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/views/view.h"

namespace remoting {

namespace {

using ash::curtain::FilterResult;

FilterResult OnlyEventsFromSource(ui::EventDeviceId source_device_id,
                                  const ui::Event& event) {
  return event.source_device_id() == source_device_id
             ? FilterResult::kKeepEvent
             : FilterResult::kSuppressEvent;
}

std::unique_ptr<views::View> CreateCurtainOverlay() {
  return std::make_unique<ash::curtain::RemoteMaintenanceCurtainView>();
}

}  // namespace

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
  ash::curtain::SecurityCurtainController::InitParams params{
      /*event_filter=*/base::BindRepeating(OnlyEventsFromSource,
                                           ui::ED_REMOTE_INPUT_DEVICE),
      /*curtain_factory=*/base::BindRepeating(CreateCurtainOverlay)};

  security_curtain_controller().Enable(params);
}

ash::curtain::SecurityCurtainController&
CurtainModeChromeOs::Core::security_curtain_controller() {
  return AshProxy::Get().GetSecurityCurtainController();
}

}  // namespace remoting
