// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_COMMON_H_
#define SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_COMMON_H_

#include <assert.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "services/device/geolocation/wifi_data_provider.h"
#include "services/device/geolocation/wifi_polling_policy.h"

namespace device {

// Converts a MAC address stored as an array of uint8_t to a string.
std::string MacAddressAsString(const uint8_t mac_as_int[6]);

// Base class to promote code sharing between platform specific wifi data
// providers. It's optional for specific platforms to derive this, but if they
// do polling behavior is taken care of by this base class, and all the platform
// need do is provide the underlying WLAN access API and polling policy.
// Also designed this way for ease of testing the cross-platform behavior.
class WifiDataProviderCommon : public WifiDataProvider {
 public:
  // Interface to abstract the low level data OS library call, and to allow
  // mocking (hence public).
  class WlanApiInterface {
   public:
    virtual ~WlanApiInterface() {}
    // Gets wifi data for all visible access points.
    virtual bool GetAccessPointData(WifiData::AccessPointDataSet* data) = 0;
  };

  WifiDataProviderCommon();

  WifiDataProviderCommon(const WifiDataProviderCommon&) = delete;
  WifiDataProviderCommon& operator=(const WifiDataProviderCommon&) = delete;

  // WifiDataProvider implementation
  void StartDataProvider() override;
  void StopDataProvider() override;
  bool DelayedByPolicy() override;
  bool GetData(WifiData* data) override;
  void ForceRescan() override;

 protected:
  ~WifiDataProviderCommon() override;

  // Returns ownership.
  virtual std::unique_ptr<WlanApiInterface> CreateWlanApi() = 0;
  // Returns ownership.
  virtual std::unique_ptr<WifiPollingPolicy> CreatePollingPolicy() = 0;

 private:
  // Runs a scan. Calls the callbacks if new data is found.
  void DoWifiScanTask();

  // Will schedule a scan; i.e. enqueue DoWifiScanTask deferred task.
  void ScheduleNextScan(int interval);

  WifiData wifi_data_;

  // Whether we've successfully completed a scan for WiFi data.
  bool is_first_scan_complete_ = false;

  // Whether our first scan was delayed due to polling policy.
  bool first_scan_delayed_ = false;

  // Underlying OS wifi API.
  std::unique_ptr<WlanApiInterface> wlan_api_;

  // Holder for delayed tasks; takes care of cleanup.
  base::WeakPtrFactory<WifiDataProviderCommon> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_COMMON_H_
