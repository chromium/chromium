// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/haptics/haptics_manager_impl.h"

#include "content/public/browser/device_service.h"
#include "services/device/public/mojom/device_service.mojom.h"

namespace content {

HapticsManagerImpl::HapticsManagerImpl() = default;

HapticsManagerImpl::~HapticsManagerImpl() = default;

void HapticsManagerImpl::PlayHaptics(device::mojom::HapticEffect effect,
                                     double intensity) {
  if (!device_haptics_service_.is_bound()) {
    GetDeviceService().BindHapticsService(
        device_haptics_service_.BindNewPipeAndPassReceiver());
    device_haptics_service_.reset_on_disconnect();
  }

  device_haptics_service_->PlayHaptics(effect, intensity);
}

}  // namespace content
