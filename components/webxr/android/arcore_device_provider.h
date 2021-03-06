// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBXR_ANDROID_ARCORE_DEVICE_PROVIDER_H_
#define COMPONENTS_WEBXR_ANDROID_ARCORE_DEVICE_PROVIDER_H_

#include <memory>

#include "base/macros.h"
#include "components/webxr/android/ar_compositor_delegate_provider.h"
#include "device/vr/public/cpp/vr_device_provider.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace device {

class ArCoreDevice;

}

namespace webxr {

class ArCoreDeviceProvider : public device::VRDeviceProvider {
 public:
  explicit ArCoreDeviceProvider(
      webxr::ArCompositorDelegateProvider compositor_delegate_provider);
  ~ArCoreDeviceProvider() override;
  void Initialize(
      base::RepeatingCallback<void(
          device::mojom::XRDeviceId,
          device::mojom::VRDisplayInfoPtr,
          device::mojom::XRDeviceDataPtr,
          mojo::PendingRemote<device::mojom::XRRuntime>)> add_device_callback,
      base::RepeatingCallback<void(device::mojom::XRDeviceId)>
          remove_device_callback,
      base::OnceClosure initialization_complete,
      device::XrFrameSinkClientFactory xr_frame_sink_client_factory) override;
  bool Initialized() override;

 private:
  webxr::ArCompositorDelegateProvider compositor_delegate_provider_;

  std::unique_ptr<device::ArCoreDevice> arcore_device_;
  bool initialized_ = false;
  DISALLOW_COPY_AND_ASSIGN(ArCoreDeviceProvider);
};

}  // namespace webxr

#endif  // COMPONENTS_WEBXR_ANDROID_ARCORE_DEVICE_PROVIDER_H_
