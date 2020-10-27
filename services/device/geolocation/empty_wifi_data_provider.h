// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_EMPTY_WIFI_DATA_PROVIDER_H_
#define SERVICES_DEVICE_GEOLOCATION_EMPTY_WIFI_DATA_PROVIDER_H_

#include "base/macros.h"
#include "services/device/geolocation/wifi_data_provider.h"

namespace device {

// An implementation of WifiDataProvider that does not provide any
// data. Used on platforms where a real implementation is not available.
class EmptyWifiDataProvider : public WifiDataProvider {
 public:
  EmptyWifiDataProvider();

  // WifiDataProvider implementation
  void StartDataProvider() override {}
  void StopDataProvider() override {}
  bool DelayedByPolicy() override;
  bool GetData(WifiData* data) override;

 private:
  ~EmptyWifiDataProvider() override;

  DISALLOW_COPY_AND_ASSIGN(EmptyWifiDataProvider);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_EMPTY_WIFI_DATA_PROVIDER_H_
