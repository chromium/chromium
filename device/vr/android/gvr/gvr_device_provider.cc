// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_device_provider.h"

#include "base/android/build_info.h"
#include "base/android/bundle_utils.h"
#include "device/vr/android/gvr/gvr_device.h"

namespace device {

GvrDeviceProvider::GvrDeviceProvider() = default;
GvrDeviceProvider::~GvrDeviceProvider() = default;

void GvrDeviceProvider::Initialize(
    base::RepeatingCallback<void(mojom::XRDeviceId,
                                 mojom::VRDisplayInfoPtr,
                                 mojo::PendingRemote<mojom::XRRuntime>)>
        add_device_callback,
    base::RepeatingCallback<void(mojom::XRDeviceId)> remove_device_callback,
    base::OnceClosure initialization_complete) {
  // Version check should match MIN_SDK_VERSION in VrCoreVersionChecker.java.
  // We only expose GvrDevice if
  //  - we could potentially install VRServices to support presentation, and
  //  - this build is a bundle and, thus, supports installing the VR module.
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
          base::android::SDK_VERSION_LOLLIPOP &&
      base::android::BundleUtils::IsBundle()) {
    vr_device_ = base::WrapUnique(new GvrDevice());
  }
  if (vr_device_) {
    add_device_callback.Run(vr_device_->GetId(), vr_device_->GetVRDisplayInfo(),
                            vr_device_->BindXRRuntime());
  }
  initialized_ = true;
  std::move(initialization_complete).Run();
}

bool GvrDeviceProvider::Initialized() {
  return initialized_;
}

}  // namespace device
