// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/display_rotation_observer.h"

#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

namespace media {

ScreenObserverDelegate::ScreenObserverDelegate(
    OnScreenRotationChangedCallback on_screen_rotation_changed_callback)
    : on_screen_rotation_changed_callback_(
          on_screen_rotation_changed_callback) {
  if (!ash::mojo_service_manager::IsServiceManagerBound()) {
    return;
  }
  ash::mojo_service_manager::GetServiceManagerProxy()->Request(
      /*service_name=*/chromeos::mojo_services::kCrosSystemEventMonitor,
      std::nullopt, monitor_.BindNewPipeAndPassReceiver().PassPipe());
  monitor_->AddDisplayObserver(receiver_.BindNewPipeAndPassRemote());
}

ScreenObserverDelegate::~ScreenObserverDelegate() = default;

void ScreenObserverDelegate::OnDisplayRotationChanged(
    cros::mojom::ClockwiseRotation rotation) {
  int display_rotation;
  switch (rotation) {
    case cros::mojom::ClockwiseRotation::kRotate0:
      display_rotation = 0;
      break;
    case cros::mojom::ClockwiseRotation::kRotate90:
      display_rotation = 90;
      break;
    case cros::mojom::ClockwiseRotation::kRotate180:
      display_rotation = 180;
      break;
    case cros::mojom::ClockwiseRotation::kRotate270:
      display_rotation = 270;
      break;
  }
  on_screen_rotation_changed_callback_.Run(display_rotation);
}

}  // namespace media
