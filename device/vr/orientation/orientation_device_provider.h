// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ORIENTATION_ORIENTATION_DEVICE_PROVIDER_H_
#define DEVICE_VR_ORIENTATION_ORIENTATION_DEVICE_PROVIDER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "device/vr/orientation/orientation_device.h"
#include "device/vr/public/cpp/vr_device_provider.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"

namespace device {

class COMPONENT_EXPORT(VR_ORIENTATION) VROrientationDeviceProvider
    : public VRDeviceProvider {
 public:
  explicit VROrientationDeviceProvider(
      mojo::PendingRemote<device::mojom::SensorProvider> sensor_provider);

  VROrientationDeviceProvider(const VROrientationDeviceProvider&) = delete;
  VROrientationDeviceProvider& operator=(const VROrientationDeviceProvider&) =
      delete;

  ~VROrientationDeviceProvider() override;

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
  void DeviceInitialized();

  bool initialized_ = false;

  mojo::Remote<device::mojom::SensorProvider> sensor_provider_;

  std::unique_ptr<VROrientationDevice> device_;

  base::RepeatingCallback<void(mojom::XRDeviceId,
                               mojom::VRDisplayInfoPtr,
                               mojom::XRDeviceDataPtr,
                               mojo::PendingRemote<mojom::XRRuntime>)>
      add_device_callback_;
  base::OnceClosure initialized_callback_;
};

}  // namespace device

#endif  // DEVICE_VR_ORIENTATION_ORIENTATION_DEVICE_PROVIDER_H_
