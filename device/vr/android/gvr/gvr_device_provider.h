// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_GVR_DEVICE_PROVIDER_H_
#define DEVICE_VR_ANDROID_GVR_GVR_DEVICE_PROVIDER_H_

#include <memory>

#include "device/vr/public/cpp/vr_device_provider.h"
#include "device/vr/vr_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace device {

class GvrDevice;

class DEVICE_VR_EXPORT GvrDeviceProvider : public VRDeviceProvider {
 public:
  GvrDeviceProvider();

  GvrDeviceProvider(const GvrDeviceProvider&) = delete;
  GvrDeviceProvider& operator=(const GvrDeviceProvider&) = delete;

  ~GvrDeviceProvider() override;

  void Initialize(VRDeviceProviderClient* client) override;
  bool Initialized() override;

 private:
  std::unique_ptr<GvrDevice> vr_device_;
  bool initialized_ = false;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_GVR_DEVICE_PROVIDER_H_
