// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/curtain_mode_chromeos.h"

#include <memory>

#include "ash/curtain/remote_maintenance_curtain_view.h"
#include "ash/curtain/security_curtain_controller.h"
#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "remoting/host/chromeos/ash_proxy.h"
#include "remoting/host/chromeos/features.h"
#include "ui/views/view.h"

namespace remoting {

namespace {

using ash::curtain::SecurityCurtainController;
using remoting::features::kEnableCrdAdminRemoteAccessV2;

std::unique_ptr<views::View> CreateCurtainOverlay() {
  return std::make_unique<ash::curtain::RemoteMaintenanceCurtainView>();
}

base::TimeDelta MuteAudioOutputDelay() {
  if (AshProxy::Get().IsScreenReaderEnabled()) {
    // Delay muting audio output by 20 seconds so the screen reader has time to
    // read the security curtain content. The default English voice takes 10
    // seconds to read the message, multiply that by two as a buffer.
    //
    // Ideally the screen reader would notify observers when it finishes the
    // alert, but the accessibility APIs do not support this. See details in
    // b/311381120.
    return base::Seconds(20);
  }
  return base::TimeDelta();
}

}  // namespace

// static
std::unique_ptr<CurtainMode> CurtainMode::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control) {
  return std::make_unique<CurtainModeChromeOs>(ui_task_runner);
}

// static
SecurityCurtainController::InitParams CurtainModeChromeOs::CreateInitParams() {
  SecurityCurtainController::InitParams params{
      /*curtain_factory=*/base::BindRepeating(CreateCurtainOverlay),
  };
  params.mute_audio_output_after = MuteAudioOutputDelay();
  if (base::FeatureList::IsEnabled(kEnableCrdAdminRemoteAccessV2)) {
    params.mute_audio_input = true;
    params.disable_camera_access = true;
  } else {
    params.mute_audio_input = false;
    params.disable_camera_access = false;
  }

  return params;
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
  // Ensure a security curtain is started with our `InitParams`, so terminate
  // any previously present curtain.
  if (security_curtain_controller().IsEnabled()) {
    security_curtain_controller().Disable();
  }
  security_curtain_controller().Enable(CreateInitParams());
}

SecurityCurtainController&
CurtainModeChromeOs::Core::security_curtain_controller() {
  return AshProxy::Get().GetSecurityCurtainController();
}

}  // namespace remoting
