// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides wifi scan API binding for chromeos, using proprietary APIs.

#include "services/device/geolocation/wifi_data_provider_chromeos.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/network/geolocation_handler.h"
#include "chromeos/network/network_handler.h"
#include "services/device/geolocation/wifi_data_provider_manager.h"

using chromeos::NetworkHandler;

namespace device {

namespace {

// The time periods between successive polls of the wifi data.
const int kDefaultPollingIntervalMilliseconds = 10 * 1000;           // 10s
const int kNoChangePollingIntervalMilliseconds = 2 * 60 * 1000;      // 2 mins
const int kTwoNoChangePollingIntervalMilliseconds = 10 * 60 * 1000;  // 10 mins
const int kNoWifiPollingIntervalMilliseconds = 20 * 1000;            // 20s

}  // namespace

WifiDataProviderChromeOs::WifiDataProviderChromeOs() = default;

WifiDataProviderChromeOs::~WifiDataProviderChromeOs() = default;

void WifiDataProviderChromeOs::StartDataProvider() {
  DCHECK(CalledOnClientThread());

  if (!WifiPollingPolicy::IsInitialized())
    WifiPollingPolicy::Initialize(CreatePollingPolicy());
  DCHECK(WifiPollingPolicy::IsInitialized());

  ScheduleStart();
}

void WifiDataProviderChromeOs::StopDataProvider() {
  DCHECK(CalledOnClientThread());
  ScheduleStop();
}

bool WifiDataProviderChromeOs::DelayedByPolicy() {
  DCHECK(CalledOnClientThread());
  return is_first_scan_complete_ ? true : first_scan_delayed_;
}

bool WifiDataProviderChromeOs::GetData(WifiData* data) {
  DCHECK(CalledOnClientThread());
  DCHECK(data);
  *data = wifi_data_;
  return is_first_scan_complete_;
}

// There is currently no reason to force a rescan on ChromeOS so this has not
// been implemented.
void WifiDataProviderChromeOs::ForceRescan() {}

std::unique_ptr<WifiPollingPolicy>
WifiDataProviderChromeOs::CreatePollingPolicy() {
  return std::make_unique<GenericWifiPollingPolicy<
      kDefaultPollingIntervalMilliseconds, kNoChangePollingIntervalMilliseconds,
      kTwoNoChangePollingIntervalMilliseconds,
      kNoWifiPollingIntervalMilliseconds>>();
}

void WifiDataProviderChromeOs::DoWifiScanTaskOnNetworkHandlerThread() {
  // This method could be scheduled after a ScheduleStop.
  if (!started_)
    return;

  WifiData new_data;

  if (GetAccessPointData(&new_data.access_point_data)) {
    client_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&WifiDataProviderChromeOs::DidWifiScanTask,
                                  this, new_data));
  } else {
    client_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&WifiDataProviderChromeOs::DidWifiScanTaskNoResults,
                       this));
  }
}

void WifiDataProviderChromeOs::DidWifiScanTaskNoResults() {
  DCHECK(CalledOnClientThread());
  // Schedule next scan if started (StopDataProvider could have been called
  // in between DoWifiScanTaskOnNetworkHandlerThread and this method).
  if (started_)
    ScheduleNextScan(WifiPollingPolicy::Get()->NoWifiInterval());
}

void WifiDataProviderChromeOs::DidWifiScanTask(const WifiData& new_data) {
  DCHECK(CalledOnClientThread());
  bool update_available = wifi_data_.DiffersSignificantly(new_data);
  wifi_data_ = new_data;
  // Schedule next scan if started (StopDataProvider could have been called
  // in between DoWifiScanTaskOnNetworkHandlerThread and this method).
  if (started_) {
    WifiPollingPolicy::Get()->UpdatePollingInterval(update_available);
    ScheduleNextScan(WifiPollingPolicy::Get()->PollingInterval());
  }

  if (update_available || !is_first_scan_complete_) {
    is_first_scan_complete_ = true;
    RunCallbacks();
  }
}

void WifiDataProviderChromeOs::ScheduleNextScan(int interval) {
  DCHECK(CalledOnClientThread());
  DCHECK(started_);
  if (!NetworkHandler::IsInitialized()) {
    LOG(ERROR) << "ScheduleNextScan called with uninitialized NetworkHandler";
    return;
  }
  NetworkHandler::Get()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &WifiDataProviderChromeOs::DoWifiScanTaskOnNetworkHandlerThread,
          this),
      base::TimeDelta::FromMilliseconds(interval));
}

void WifiDataProviderChromeOs::ScheduleStop() {
  DCHECK(CalledOnClientThread());
  DCHECK(started_);
  started_ = false;
}

void WifiDataProviderChromeOs::ScheduleStart() {
  DCHECK(CalledOnClientThread());
  DCHECK(!started_);
  if (!NetworkHandler::IsInitialized()) {
    LOG(ERROR) << "ScheduleStart called with uninitialized NetworkHandler";
    return;
  }
  started_ = true;
  int delay_interval = WifiPollingPolicy::Get()->InitialInterval();
  ScheduleNextScan(delay_interval);
  first_scan_delayed_ = (delay_interval > 0);
}

bool WifiDataProviderChromeOs::GetAccessPointData(
    WifiData::AccessPointDataSet* result) {
  // If in startup or shutdown, NetworkHandler is uninitialized.
  if (!NetworkHandler::IsInitialized())
    return false;  // Data not ready.

  DCHECK(NetworkHandler::Get()->task_runner()->BelongsToCurrentThread());

  // If wifi isn't enabled, we've effectively completed the task.
  chromeos::GeolocationHandler* const geolocation_handler =
      NetworkHandler::Get()->geolocation_handler();
  if (!geolocation_handler || !geolocation_handler->wifi_enabled())
    return true;  // Access point list is empty, no more data.

  chromeos::WifiAccessPointVector access_points;
  int64_t age_ms = 0;
  if (!geolocation_handler->GetWifiAccessPoints(&access_points, &age_ms))
    return false;

  for (const auto& access_point : access_points) {
    AccessPointData ap_data;
    ap_data.mac_address = base::ASCIIToUTF16(access_point.mac_address);
    ap_data.radio_signal_strength = access_point.signal_strength;
    ap_data.channel = access_point.channel;
    ap_data.signal_to_noise = access_point.signal_to_noise;
    ap_data.ssid = base::UTF8ToUTF16(access_point.ssid);
    result->insert(ap_data);
  }
  // If the age is significantly longer than our long polling time, assume the
  // data is stale and return false which will trigger a faster update.
  if (age_ms > kTwoNoChangePollingIntervalMilliseconds * 2)
    return false;
  return true;
}

// static
WifiDataProvider* WifiDataProviderManager::DefaultFactoryFunction() {
  return new WifiDataProviderChromeOs();
}

}  // namespace device
