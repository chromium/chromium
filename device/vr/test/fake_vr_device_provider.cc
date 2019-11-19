// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/test/fake_vr_device_provider.h"
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
    add_device_callback_.Run(device_base->GetId(),
                             device_base->GetVRDisplayInfo(),
                             device_base->BindXRRuntime());
}

void FakeVRDeviceProvider::RemoveDevice(mojom::XRDeviceId device_id) {
  auto it = std::find_if(
      devices_.begin(), devices_.end(),
      [device_id](const std::unique_ptr<VRDeviceBase>& device) {
        return static_cast<VRDeviceBase*>(device.get())->GetId() == device_id;
      });
  if (initialized_)
    remove_device_callback_.Run(device_id);
  devices_.erase(it);
}

void FakeVRDeviceProvider::Initialize(
    base::RepeatingCallback<void(mojom::XRDeviceId,
                                 mojom::VRDisplayInfoPtr,
                                 mojo::PendingRemote<mojom::XRRuntime>)>
        add_device_callback,
    base::RepeatingCallback<void(mojom::XRDeviceId)> remove_device_callback,
    base::OnceClosure initialization_complete) {
  add_device_callback_ = std::move(add_device_callback);
  remove_device_callback_ = std::move(remove_device_callback);

  for (std::unique_ptr<VRDeviceBase>& device : devices_) {
    auto* device_base = static_cast<VRDeviceBase*>(device.get());
    add_device_callback_.Run(device_base->GetId(),
                             device_base->GetVRDisplayInfo(),
                             device_base->BindXRRuntime());
  }
  initialized_ = true;
  std::move(initialization_complete).Run();
}

bool FakeVRDeviceProvider::Initialized() {
  return initialized_;
}

}  // namespace device
