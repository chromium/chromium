// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_device_provider.h"

#include "base/android/build_info.h"
#include "base/android/bundle_utils.h"
#include "base/memory/ptr_util.h"
#include "device/vr/android/gvr/gvr_device.h"

namespace device {

GvrDeviceProvider::GvrDeviceProvider() = default;
GvrDeviceProvider::~GvrDeviceProvider() = default;

void GvrDeviceProvider::Initialize(VRDeviceProviderClient* client) {
  // We only expose GvrDevice if
  //  - we could potentially install VRServices to support presentation, and
  //  - this build is a bundle and, thus, supports installing the VR module.
  if (base::android::BundleUtils::IsBundle()) {
    vr_device_ = base::WrapUnique(new GvrDevice());
  }
  if (vr_device_) {
    client->AddRuntime(vr_device_->GetId(), vr_device_->GetDeviceData(),
                       vr_device_->BindXRRuntime());
  }
  initialized_ = true;
  client->OnProviderInitialized();
}

bool GvrDeviceProvider::Initialized() {
  return initialized_;
}

}  // namespace device
