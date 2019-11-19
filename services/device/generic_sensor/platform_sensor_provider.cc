// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_provider.h"

#if defined(OS_MACOSX)
#include "services/device/generic_sensor/platform_sensor_provider_mac.h"
#elif defined(OS_ANDROID)
#include "services/device/generic_sensor/platform_sensor_provider_android.h"
#elif defined(OS_WIN)
#include "base/feature_list.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "services/device/generic_sensor/platform_sensor_provider_win.h"
#include "services/device/generic_sensor/platform_sensor_provider_winrt.h"
#include "services/device/public/cpp/device_features.h"
#elif defined(OS_LINUX) && defined(USE_UDEV)
#include "services/device/generic_sensor/platform_sensor_provider_linux.h"
#endif

namespace device {

// static
std::unique_ptr<PlatformSensorProvider> PlatformSensorProvider::Create() {
#if defined(OS_MACOSX)
  return std::make_unique<PlatformSensorProviderMac>();
#elif defined(OS_ANDROID)
  return std::make_unique<PlatformSensorProviderAndroid>();
#elif defined(OS_WIN)
  if (PlatformSensorProvider::UseWindowsWinrt()) {
    return std::make_unique<PlatformSensorProviderWinrt>();
  } else {
    return std::make_unique<PlatformSensorProviderWin>();
  }
#elif defined(OS_LINUX) && defined(USE_UDEV)
  return std::make_unique<PlatformSensorProviderLinux>();
#else
  return nullptr;
#endif
}

#if defined(OS_WIN)
// static
bool PlatformSensorProvider::UseWindowsWinrt() {
  // TODO: Windows version dependency should eventually be updated to
  // a future version which supports WinRT sensor thresholding. Since
  // this Windows version has yet to be released, Win10 is being
  // provisionally used for testing. This also means sensors will
  // stream if this implementation path is enabled.
  return base::FeatureList::IsEnabled(features::kWinrtSensorsImplementation) &&
         base::win::GetVersion() >= base::win::Version::WIN10;
}
#endif

}  // namespace device
