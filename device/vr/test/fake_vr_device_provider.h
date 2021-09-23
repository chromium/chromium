// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_TEST_FAKE_VR_DEVICE_PROVIDER_H_
#define DEVICE_VR_TEST_FAKE_VR_DEVICE_PROVIDER_H_

#include <vector>

#include "device/vr/public/cpp/vr_device_provider.h"
#include "device/vr/vr_device_base.h"
#include "device/vr/vr_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace device {

// TODO(mthiesse, crbug.com/769373): Remove DEVICE_VR_EXPORT.
class DEVICE_VR_EXPORT FakeVRDeviceProvider : public VRDeviceProvider {
 public:
  FakeVRDeviceProvider();

  FakeVRDeviceProvider(const FakeVRDeviceProvider&) = delete;
  FakeVRDeviceProvider& operator=(const FakeVRDeviceProvider&) = delete;

  ~FakeVRDeviceProvider() override;

  // Adds devices to the provider with the given device, which will be
  // returned when GetDevices is queried.
  void AddDevice(std::unique_ptr<VRDeviceBase> device);
  void RemoveDevice(mojom::XRDeviceId device_id);

  void Initialize(
      base::RepeatingCallback<void(mojom::XRDeviceId,
                                   mojom::VRDisplayInfoPtr,
                                   mojom::XRDeviceDataPtr,
                                   mojo::PendingRemote<mojom::XRRuntime>)>
          add_device_callback,
      base::RepeatingCallback<void(mojom::XRDeviceId)> remove_device_callback,
      base::OnceClosure initialization_complete,
      XrFrameSinkClientFactory xr_frame_sink_client_factory) override;
  bool Initialized() override;

 private:
  std::vector<std::unique_ptr<VRDeviceBase>> devices_;
  bool initialized_;
  base::RepeatingCallback<void(mojom::XRDeviceId,
                               mojom::VRDisplayInfoPtr,
                               mojom::XRDeviceDataPtr,
                               mojo::PendingRemote<mojom::XRRuntime>)>
      add_device_callback_;
  base::RepeatingCallback<void(mojom::XRDeviceId)> remove_device_callback_;
};

}  // namespace device

#endif  // DEVICE_VR_TEST_FAKE_VR_DEVICE_PROVIDER_H_
