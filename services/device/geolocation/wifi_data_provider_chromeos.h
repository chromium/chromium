// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_CHROMEOS_H_
#define SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_CHROMEOS_H_

#include <memory>
#include <optional>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "services/device/geolocation/wifi_data_provider.h"
#include "services/device/geolocation/wifi_polling_policy.h"

namespace device {

class WifiDataProviderChromeOs : public WifiDataProvider {
 public:
  WifiDataProviderChromeOs();

  WifiDataProviderChromeOs(const WifiDataProviderChromeOs&) = delete;
  WifiDataProviderChromeOs& operator=(const WifiDataProviderChromeOs&) = delete;

  // WifiDataProvider
  void StartDataProvider() override;
  void StopDataProvider() override;
  bool DelayedByPolicy() override;
  bool GetData(WifiData* data) override;
  void ForceRescan() override;

  std::optional<WifiData> GetWifiDataForTesting();

 private:
  friend class GeolocationChromeOsWifiDataProviderTest;
  ~WifiDataProviderChromeOs() override;

  // Returns ownership.
  std::unique_ptr<WifiPollingPolicy> CreatePollingPolicy();

  void DoWifiScanTask();
  void OnWifiScanTaskComplete(std::optional<WifiData> wifi_data);

  // Will schedule a scan; i.e. enqueue DoWifiScanTask deferred task.
  void ScheduleNextScan(int interval);

  // Will schedule starting of the scanning process.
  void ScheduleStart();

  // Will schedule stopping of the scanning process.
  void ScheduleStop();

  // The latest Wi-Fi data.
  WifiData wifi_data_;

  // Whether we have started the data provider.
  bool started_ = false;

  // Whether we've successfully completed a scan for Wi-Fi data.
  bool is_first_scan_complete_ = false;

  // Whether our first scan was delayed due to polling policy.
  bool first_scan_delayed_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WifiDataProviderChromeOs> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_CHROMEOS_H_
