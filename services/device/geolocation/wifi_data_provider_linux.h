// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_LINUX_H_
#define SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_LINUX_H_

#include <memory>

#include "services/device/geolocation/wifi_data_provider_common.h"

namespace dbus {
class Bus;
}

namespace device {

class WifiDataProviderLinux : public WifiDataProviderCommon {
 public:
  WifiDataProviderLinux();

  WifiDataProviderLinux(const WifiDataProviderLinux&) = delete;
  WifiDataProviderLinux& operator=(const WifiDataProviderLinux&) = delete;

 private:
  friend class GeolocationWifiDataProviderLinuxTest;

  ~WifiDataProviderLinux() override;

  // WifiDataProviderCommon implementation
  std::unique_ptr<WlanApiInterface> CreateWlanApi() override;
  std::unique_ptr<WifiPollingPolicy> CreatePollingPolicy() override;

  std::unique_ptr<WlanApiInterface> CreateWlanApiForTesting(
      scoped_refptr<dbus::Bus> bus);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_LINUX_H_
