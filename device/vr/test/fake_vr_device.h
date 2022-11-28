// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_TEST_FAKE_VR_DEVICE_H_
#define DEVICE_VR_TEST_FAKE_VR_DEVICE_H_

#include "device/vr/public/cpp/vr_device_provider.h"
#include "device/vr/vr_device_base.h"
#include "device/vr/vr_export.h"

namespace device {

// TODO(mthiesse, crbug.com/769373): Remove DEVICE_VR_EXPORT.
class DEVICE_VR_EXPORT FakeVRDevice : public VRDeviceBase,
                                      public mojom::XRSessionController {
 public:
  explicit FakeVRDevice(mojom::XRDeviceId id);

  FakeVRDevice(const FakeVRDevice&) = delete;
  FakeVRDevice& operator=(const FakeVRDevice&) = delete;

  ~FakeVRDevice() override;

  void RequestSession(
      mojom::XRRuntimeSessionOptionsPtr options,
      mojom::XRRuntime::RequestSessionCallback callback) override;
  void ShutdownSession(
      mojom::XRRuntime::ShutdownSessionCallback callback) override;
  void SetPose(mojom::VRPosePtr pose) { pose_ = std::move(pose); }

  void SetFrameDataRestricted(bool restricted) override {}

  using VRDeviceBase::IsPresenting;  // Make it public for tests.

  void StopSession() { OnPresentingControllerMojoConnectionError(); }

 private:
  void OnPresentingControllerMojoConnectionError();

  mojom::VRPosePtr pose_;
};

}  // namespace device

#endif  // DEVICE_VR_TEST_FAKE_VR_DEVICE_H_
