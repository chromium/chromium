// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screenlock_monitor/screenlock_monitor_device_source.h"

#include "chromeos/crosapi/mojom/login_state.mojom.h"
#include "chromeos/lacros/lacros_service.h"

namespace content {

ScreenlockMonitorDeviceSource::ScreenLockListener::ScreenLockListener()
    : receiver_(this) {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service ||
      !lacros_service->IsAvailable<crosapi::mojom::LoginState>()) {
    LOG(WARNING) << "crosapi: LoginState not available.";
    return;
  }

  lacros_service->GetRemote<crosapi::mojom::LoginState>()->AddObserver(
      receiver_.BindNewPipeAndPassRemote());
}

ScreenlockMonitorDeviceSource::ScreenLockListener::~ScreenLockListener() =
    default;

void ScreenlockMonitorDeviceSource::ScreenLockListener::OnSessionStateChanged(
    crosapi::mojom::SessionState state) {
  ScreenlockEvent screenlock_event;
  if (state == crosapi::mojom::SessionState::kInLockScreen) {
    screenlock_event = SCREEN_LOCK_EVENT;
  } else {
    screenlock_event = SCREEN_UNLOCK_EVENT;
  }

  if (!prev_event_ || *prev_event_ != screenlock_event) {
    prev_event_ = screenlock_event;
    ProcessScreenlockEvent(screenlock_event);
  }
}

}  // namespace content
