// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_CPP_VR_DEVICE_PROVIDER_H_
#define DEVICE_VR_PUBLIC_CPP_VR_DEVICE_PROVIDER_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "device/vr/public/cpp/xr_frame_sink_client.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace device {

class COMPONENT_EXPORT(VR_PUBLIC_CPP) VRDeviceProviderClient {
 public:
  VRDeviceProviderClient();
  virtual ~VRDeviceProviderClient();

  virtual void AddRuntime(
      device::mojom::XRDeviceId id,
      device::mojom::XRDeviceDataPtr device_data,
      mojo::PendingRemote<device::mojom::XRRuntime> runtime) = 0;
  virtual void RemoveRuntime(device::mojom::XRDeviceId id) = 0;
  virtual void OnProviderInitialized() = 0;
  virtual XrFrameSinkClientFactory GetXrFrameSinkClientFactory() = 0;
};

class COMPONENT_EXPORT(VR_PUBLIC_CPP) VRDeviceProvider {
 public:
  VRDeviceProvider();
  virtual ~VRDeviceProvider();

  // If the VR API requires initialization that should happen here.
  // Note that the client must be guaranteed to outlive the device provider.
  virtual void Initialize(VRDeviceProviderClient* client) = 0;

  // Returns true if initialization is complete.
  virtual bool Initialized() = 0;
};

}  // namespace device

#endif  // DEVICE_VR_PUBLIC_CPP_VR_DEVICE_PROVIDER_H_
