// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_CPP_VR_DEVICE_PROVIDER_H_
#define DEVICE_VR_PUBLIC_CPP_VR_DEVICE_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "device/vr/public/cpp/xr_frame_sink_client.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace device {

class COMPONENT_EXPORT(VR_PUBLIC_CPP) VRDeviceProvider {
 public:
  VRDeviceProvider();
  virtual ~VRDeviceProvider();

  // If the VR API requires initialization that should happen here.
  virtual void Initialize(
      base::RepeatingCallback<void(mojom::XRDeviceId id,
                                   mojom::VRDisplayInfoPtr,
                                   mojom::XRDeviceDataPtr,
                                   mojo::PendingRemote<mojom::XRRuntime>)>
          add_device_callback,
      base::RepeatingCallback<void(mojom::XRDeviceId id)>
          remove_device_callback,
      base::OnceClosure initialization_complete,
      XrFrameSinkClientFactory xr_frame_sink_client_factory) = 0;

  // Returns true if initialization is complete.
  virtual bool Initialized() = 0;
};

}  // namespace device

#endif  // DEVICE_VR_PUBLIC_CPP_VR_DEVICE_PROVIDER_H_
