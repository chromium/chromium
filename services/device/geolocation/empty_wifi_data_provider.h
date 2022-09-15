// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_EMPTY_WIFI_DATA_PROVIDER_H_
#define SERVICES_DEVICE_GEOLOCATION_EMPTY_WIFI_DATA_PROVIDER_H_

#include "services/device/geolocation/wifi_data_provider.h"

namespace device {

// An implementation of WifiDataProvider that does not provide any
// data. Used on platforms where a real implementation is not available.
class EmptyWifiDataProvider : public WifiDataProvider {
 public:
  EmptyWifiDataProvider();

  EmptyWifiDataProvider(const EmptyWifiDataProvider&) = delete;
  EmptyWifiDataProvider& operator=(const EmptyWifiDataProvider&) = delete;

  // WifiDataProvider implementation
  void StartDataProvider() override {}
  void StopDataProvider() override {}
  bool DelayedByPolicy() override;
  bool GetData(WifiData* data) override;
  void ForceRescan() override;

 private:
  ~EmptyWifiDataProvider() override;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_EMPTY_WIFI_DATA_PROVIDER_H_
