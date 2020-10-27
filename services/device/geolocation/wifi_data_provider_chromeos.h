// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_CHROMEOS_H_
#define SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_CHROMEOS_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "services/device/geolocation/wifi_data_provider.h"
#include "services/device/geolocation/wifi_polling_policy.h"

namespace device {

class WifiDataProviderChromeOs : public WifiDataProvider {
 public:
  WifiDataProviderChromeOs();

  // WifiDataProvider
  void StartDataProvider() override;
  void StopDataProvider() override;
  bool DelayedByPolicy() override;
  bool GetData(WifiData* data) override;

 private:
  friend class GeolocationChromeOsWifiDataProviderTest;
  ~WifiDataProviderChromeOs() override;

  // Returns ownership.
  std::unique_ptr<WifiPollingPolicy> CreatePollingPolicy();

  // NetworkHandler thread
  void DoWifiScanTaskOnNetworkHandlerThread();

  // Client thread
  void DidWifiScanTaskNoResults();
  void DidWifiScanTask(const WifiData& new_data);

  // Will schedule a scan; i.e. enqueue DoWifiScanTask deferred task.
  void ScheduleNextScan(int interval);

  // Will schedule starting of the scanning process.
  void ScheduleStart();

  // Will schedule stopping of the scanning process.
  void ScheduleStop();

  // Get access point data from chromeos.
  bool GetAccessPointData(WifiData::AccessPointDataSet* data);

  // The latest wifi data. (client thread)
  WifiData wifi_data_;

  // Whether we have started the data provider. (client thread)
  bool started_ = false;

  // Whether we've successfully completed a scan for WiFi data. (client thread)
  bool is_first_scan_complete_ = false;

  // Whether our first scan was delayed due to polling policy. (client thread)
  bool first_scan_delayed_ = false;

  DISALLOW_COPY_AND_ASSIGN(WifiDataProviderChromeOs);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_CHROMEOS_H_
