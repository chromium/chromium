// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/device_posture/device_posture_platform_provider.h"

#include "build/build_config.h"
#include "services/device/device_posture/device_posture_provider_impl.h"

#if BUILDFLAG(IS_WIN)
#include "services/device/device_posture/device_posture_platform_provider_win.h"
#elif BUILDFLAG(IS_ANDROID)
#include "services/device/device_posture/device_posture_platform_provider_android.h"
#endif

namespace device {

// static
std::unique_ptr<DevicePosturePlatformProvider>
DevicePosturePlatformProvider::Create() {
#if BUILDFLAG(IS_WIN)
  return std::make_unique<DevicePosturePlatformProviderWin>();
#elif BUILDFLAG(IS_ANDROID)
  return std::make_unique<DevicePosturePlatformProviderAndroid>();
#else
  return nullptr;
#endif
}

void DevicePosturePlatformProvider::SetPostureProvider(
    DevicePostureProviderImpl* provider) {
  provider_ = provider;
}

void DevicePosturePlatformProvider::NotifyDevicePostureChanged(
    const device::mojom::DevicePostureType& posture) {
  DCHECK(provider_);
  provider_->OnDevicePostureChanged(posture);
}

}  // namespace device
