// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/test/fake_vr_device.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/gfx/geometry/transform_util.h"

namespace device {

FakeVRDevice::FakeVRDevice(mojom::XRDeviceId id) : VRDeviceBase(id) {}

FakeVRDevice::~FakeVRDevice() {}

void FakeVRDevice::RequestSession(
    mojom::XRRuntimeSessionOptionsPtr options,
    mojom::XRRuntime::RequestSessionCallback callback) {
  OnStartPresenting();
  // The current tests never use the return values, so it's fine to return
  // invalid data here.
  std::move(callback).Run(nullptr);
}

void FakeVRDevice::ShutdownSession(
    mojom::XRRuntime::ShutdownSessionCallback callback) {
  OnExitPresent();
  std::move(callback).Run();
}

void FakeVRDevice::OnPresentingControllerMojoConnectionError() {
  OnExitPresent();
}

}  // namespace device
