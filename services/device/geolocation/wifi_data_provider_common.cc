// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/geolocation/wifi_data_provider_common.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "components/device_event_log/device_event_log.h"

namespace device {

std::string MacAddressAsString(const uint8_t mac_as_int[6]) {
  // |mac_as_int| is big-endian. Write in byte chunks.
  // Format is XX-XX-XX-XX-XX-XX.
  static constexpr char kMacFormatString[] = "%02x-%02x-%02x-%02x-%02x-%02x";
  return base::StringPrintf(kMacFormatString, mac_as_int[0], mac_as_int[1],
                            mac_as_int[2], mac_as_int[3], mac_as_int[4],
                            mac_as_int[5]);
}

WifiDataProviderCommon::WifiDataProviderCommon() = default;

WifiDataProviderCommon::~WifiDataProviderCommon() = default;

void WifiDataProviderCommon::StartDataProvider() {
  DCHECK(!wlan_api_);
  wlan_api_ = CreateWlanApi();
  if (!wlan_api_) {
    // Error! Can't do scans, so don't try and schedule one.
    is_first_scan_complete_ = true;
    return;
  }

  if (!WifiPollingPolicy::IsInitialized())
    WifiPollingPolicy::Initialize(CreatePollingPolicy());
  DCHECK(WifiPollingPolicy::IsInitialized());

  int delay_interval = WifiPollingPolicy::Get()->InitialInterval();
  ScheduleNextScan(delay_interval);
  first_scan_delayed_ = (delay_interval > 0);
}

void WifiDataProviderCommon::StopDataProvider() {
  wlan_api_.reset();
}

bool WifiDataProviderCommon::DelayedByPolicy() {
  return is_first_scan_complete_ ? true : first_scan_delayed_;
}

bool WifiDataProviderCommon::GetData(WifiData* data) {
  *data = wifi_data_;
  // If we've successfully completed a scan, indicate that we have all of the
  // data we can get.
  return is_first_scan_complete_;
}

void WifiDataProviderCommon::ForceRescan() {
  DoWifiScanTask();
}

void WifiDataProviderCommon::DoWifiScanTask() {
  // Abort the wifi scan if the provider is already being torn down.
  if (!wlan_api_)
    return;

  bool update_available = false;
  WifiData new_data;
  if (!wlan_api_->GetAccessPointData(&new_data.access_point_data)) {
    ScheduleNextScan(WifiPollingPolicy::Get()->NoWifiInterval());
  } else {
    update_available = wifi_data_.DiffersSignificantly(new_data);
    wifi_data_ = new_data;
    WifiPollingPolicy::Get()->UpdatePollingInterval(update_available);
    ScheduleNextScan(WifiPollingPolicy::Get()->PollingInterval());
  }
  GEOLOCATION_LOG(DEBUG) << "Scanned: update_available=" << update_available
                         << " is_first_scan_complete_="
                         << is_first_scan_complete_;
  if (update_available || !is_first_scan_complete_) {
    is_first_scan_complete_ = true;
    RunCallbacks();
  }
}

void WifiDataProviderCommon::ScheduleNextScan(int interval) {
  GEOLOCATION_LOG(DEBUG) << "Schedule next scan: interval=" << interval << "ms";
  client_task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WifiDataProviderCommon::DoWifiScanTask,
                     weak_factory_.GetWeakPtr()),
      base::Milliseconds(interval));
}

}  // namespace device
