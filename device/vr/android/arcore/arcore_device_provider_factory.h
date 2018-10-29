// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_ARCORE_ARCORE_DEVICE_PROVIDER_FACTORY_H_
#define DEVICE_VR_ANDROID_ARCORE_ARCORE_DEVICE_PROVIDER_FACTORY_H_

#include <memory>
#include "base/macros.h"
#include "device/vr/vr_export.h"

namespace device {

class VRDeviceProvider;

class DEVICE_VR_EXPORT ArCoreDeviceProviderFactory {
 public:
  static std::unique_ptr<device::VRDeviceProvider> Create();
  static void Install(std::unique_ptr<ArCoreDeviceProviderFactory> factory);

  virtual ~ArCoreDeviceProviderFactory() = default;

 protected:
  ArCoreDeviceProviderFactory() = default;

  virtual std::unique_ptr<device::VRDeviceProvider> CreateDeviceProvider() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArCoreDeviceProviderFactory);
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_ARCORE_ARCORE_DEVICE_PROVIDER_FACTORY_H_
