// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/test/fake_vr_device_provider.h"

#include "base/ranges/algorithm.h"
#include "device/vr/vr_device_base.h"

namespace device {

FakeVRDeviceProvider::FakeVRDeviceProvider() : VRDeviceProvider() {
  initialized_ = false;
}

FakeVRDeviceProvider::~FakeVRDeviceProvider() {}

void FakeVRDeviceProvider::AddDevice(std::unique_ptr<VRDeviceBase> device) {
  VRDeviceBase* device_base = static_cast<VRDeviceBase*>(device.get());
  devices_.push_back(std::move(device));
  if (initialized_)
    client_->AddRuntime(device_base->GetId(), device_base->GetDeviceData(),
                        device_base->BindXRRuntime());
}

void FakeVRDeviceProvider::RemoveDevice(mojom::XRDeviceId device_id) {
  auto it = base::ranges::find(devices_, device_id, &VRDeviceBase::GetId);
  if (initialized_)
    client_->RemoveRuntime(device_id);
  devices_.erase(it);
}

void FakeVRDeviceProvider::Initialize(
    VRDeviceProviderClient* client,
    content::WebContents* initializing_web_contents) {
  client_ = client;

  for (std::unique_ptr<VRDeviceBase>& device : devices_) {
    auto* device_base = static_cast<VRDeviceBase*>(device.get());
    client_->AddRuntime(device_base->GetId(), device_base->GetDeviceData(),
                        device_base->BindXRRuntime());
  }
  initialized_ = true;
  client_->OnProviderInitialized();
}

bool FakeVRDeviceProvider::Initialized() {
  return initialized_;
}

}  // namespace device
