// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_MAC_H_
#define SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_MAC_H_

#include "services/device/geolocation/wifi_data_provider_common.h"

namespace device {

// Implementation of the wifi data provider for macOS using CoreWLAN.
class WifiDataProviderMac : public WifiDataProviderCommon {
 public:
  WifiDataProviderMac();

  WifiDataProviderMac(const WifiDataProviderMac&) = delete;
  WifiDataProviderMac& operator=(const WifiDataProviderMac&) = delete;

 private:
  ~WifiDataProviderMac() override;

  // WifiDataProviderCommon implementation
  std::unique_ptr<WlanApiInterface> CreateWlanApi() override;
  std::unique_ptr<WifiPollingPolicy> CreatePollingPolicy() override;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_MAC_H_
