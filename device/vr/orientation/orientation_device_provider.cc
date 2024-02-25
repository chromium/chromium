// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/orientation/orientation_device_provider.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "device/vr/orientation/orientation_device.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"

namespace device {

VROrientationDeviceProvider::VROrientationDeviceProvider(
    mojo::PendingRemote<device::mojom::SensorProvider> sensor_provider)
    : sensor_provider_(std::move(sensor_provider)) {}

VROrientationDeviceProvider::~VROrientationDeviceProvider() = default;

void VROrientationDeviceProvider::Initialize(
    VRDeviceProviderClient* client,
    content::WebContents* initializing_web_contents) {
  if (device_ && device_->IsAvailable()) {
    client->AddRuntime(device_->GetId(), device_->GetDeviceData(),
                       device_->BindXRRuntime());
    return;
  }

  if (!device_) {
    client_ = client;
    device_ = std::make_unique<VROrientationDevice>(
        sensor_provider_.get(),
        base::BindOnce(&VROrientationDeviceProvider::DeviceInitialized,
                       base::Unretained(this)));
  }
}

bool VROrientationDeviceProvider::Initialized() {
  return initialized_;
}

void VROrientationDeviceProvider::DeviceInitialized() {
  // This should only be called after the device is initialized.
  DCHECK(device_);
  // This should only be called once.
  DCHECK(!initialized_);

  // If the device successfully connected to the orientation APIs, provide it.
  if (device_->IsAvailable()) {
    client_->AddRuntime(device_->GetId(), device_->GetDeviceData(),
                        device_->BindXRRuntime());
  }

  initialized_ = true;
  client_->OnProviderInitialized();
}

}  // namespace device
