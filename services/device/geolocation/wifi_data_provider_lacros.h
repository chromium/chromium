// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_LACROS_H_
#define SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_LACROS_H_

#include <memory>

#include "chromeos/crosapi/mojom/geolocation.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/geolocation/wifi_data_provider.h"

namespace base {
class RetainingOneShotTimer;
}  // namespace base

namespace device {

// Implements the WifiDataProvider interface for the Lacros platform. Uses
// polling to query the crosapi::GeolocationService for wifi access points
// according to its WifiPollingPolicy.
class WifiDataProviderLacros : public WifiDataProvider {
 public:
  WifiDataProviderLacros();
  WifiDataProviderLacros(const WifiDataProviderLacros&) = delete;
  WifiDataProviderLacros& operator=(const WifiDataProviderLacros&) = delete;

  // WifiDataProvider:
  void StartDataProvider() override;
  void StopDataProvider() override;
  bool DelayedByPolicy() override;
  bool GetData(WifiData* data) override;
  void ForceRescan() override;

  void DidWifiScanTaskForTesting(
      bool service_initialized,
      bool data_available,
      base::TimeDelta time_since_last_updated,
      std::vector<crosapi::mojom::AccessPointDataPtr> access_points);

 private:
  ~WifiDataProviderLacros() override;

  // Will schedule a scan; i.e. start the timer for a deferred DoWifiScanTask().
  void ScheduleNextScan(int interval_ms);

  // Makes a request to ash-chrome's GeolocationService for device access
  // points.
  void DoWifiScanTask();

  // Ash-chrome's GeolocationService calls back into this in response to the
  // service request in DoWifiScanTask().
  void DidWifiScanTask(
      bool service_initialized,
      bool data_available,
      base::TimeDelta time_since_last_updated,
      std::vector<crosapi::mojom::AccessPointDataPtr> access_points);

  // The latest wifi data.
  WifiData wifi_data_;

  // Whether we have started the data provider.
  bool started_ = false;

  // Whether we've successfully completed a scan for WiFi data.
  bool is_first_scan_complete_ = false;

  // Whether our first scan was delayed due to polling policy.
  bool first_scan_delayed_ = false;

  // Used to schedule the next DoWifiScanTask().
  base::RetainingOneShotTimer wifi_scan_timer_;

  // The remote connection to the geolocation service.
  mojo::Remote<crosapi::mojom::GeolocationService> geolocation_service_;

  // Holder for delayed tasks; takes care of cleanup.
  base::WeakPtrFactory<WifiDataProviderLacros> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_LACROS_H_
