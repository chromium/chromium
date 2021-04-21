// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_GVR_DEVICE_PROVIDER_H_
#define DEVICE_VR_ANDROID_GVR_GVR_DEVICE_PROVIDER_H_

#include <memory>

#include "base/macros.h"
#include "device/vr/public/cpp/vr_device_provider.h"
#include "device/vr/vr_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace device {

class GvrDevice;

class DEVICE_VR_EXPORT GvrDeviceProvider : public VRDeviceProvider {
 public:
  GvrDeviceProvider();
  ~GvrDeviceProvider() override;

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
  std::unique_ptr<GvrDevice> vr_device_;
  bool initialized_ = false;

  DISALLOW_COPY_AND_ASSIGN(GvrDeviceProvider);
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_GVR_DEVICE_PROVIDER_H_
