// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides wifi scan API binding for chromeos, using proprietary APIs.

#include "services/device/geolocation/wifi_data_provider_chromeos.h"

#include <stdint.h>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/components/network/geolocation_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "services/device/geolocation/wifi_data_provider_handle.h"
#include "services/device/public/mojom/geolocation_internals.mojom.h"

using ::ash::NetworkHandler;

namespace device {

namespace {

// The time periods between successive polls of the wifi data.
const int kDefaultPollingIntervalMilliseconds = 10 * 1000;           // 10s
const int kNoChangePollingIntervalMilliseconds = 2 * 60 * 1000;      // 2 mins
const int kTwoNoChangePollingIntervalMilliseconds = 10 * 60 * 1000;  // 10 mins
const int kNoWifiPollingIntervalMilliseconds = 20 * 1000;            // 20s

// The mobile location service (MLS) imposes a hard-coded limit on the number of
// access points that can be used to generate a position estimate.
constexpr size_t kApUseLimit = 20;

// Returns the Wi-Fi access point data from ChromeOS, or `nullopt` if the
// NetworkHandler is not started or failed to acquire fresh data.
std::optional<WifiData> GetWifiData() {
  DCHECK(NetworkHandler::Get()->task_runner()->BelongsToCurrentThread());
  // If in startup or shutdown, NetworkHandler is uninitialized.
  if (!NetworkHandler::IsInitialized()) {
    return std::nullopt;  // Data not ready.
  }
  // If Wi-Fi isn't enabled, we've effectively completed the task.
  ash::GeolocationHandler* const geolocation_handler =
      NetworkHandler::Get()->geolocation_handler();
  if (!geolocation_handler || !geolocation_handler->wifi_enabled()) {
    // Access point list is empty, no more data.
    return WifiData();
  }
  ash::WifiAccessPointVector access_points;
  int64_t age_ms = 0;
  if (!geolocation_handler->GetWifiAccessPoints(&access_points, &age_ms)) {
    return std::nullopt;
  }
  // If the age is significantly longer than our long polling time, assume the
  // data is stale to trigger a faster update.
  if (age_ms > kTwoNoChangePollingIntervalMilliseconds * 2) {
    return std::nullopt;
  }

  // Sort AP sightings by age, most recent first.
  base::ranges::sort(access_points, base::ranges::greater(),
                     &ash::WifiAccessPoint::timestamp);

  // Truncate to kApUseLimit.
  if (access_points.size() > kApUseLimit) {
    access_points.resize(kApUseLimit);
  }

  WifiData wifi_data;
  for (const auto& access_point : access_points) {
    mojom::AccessPointData ap_data;
    ap_data.mac_address = access_point.mac_address;
    ap_data.radio_signal_strength = access_point.signal_strength;
    ap_data.channel = access_point.channel;
    ap_data.signal_to_noise = access_point.signal_to_noise;
    ap_data.timestamp = access_point.timestamp;
    wifi_data.access_point_data.insert(ap_data);
  }
  return wifi_data;
}

}  // namespace

WifiDataProviderChromeOs::WifiDataProviderChromeOs() = default;

WifiDataProviderChromeOs::~WifiDataProviderChromeOs() = default;

void WifiDataProviderChromeOs::StartDataProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!WifiPollingPolicy::IsInitialized())
    WifiPollingPolicy::Initialize(CreatePollingPolicy());
  DCHECK(WifiPollingPolicy::IsInitialized());

  ScheduleStart();
}

void WifiDataProviderChromeOs::StopDataProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleStop();
}

bool WifiDataProviderChromeOs::DelayedByPolicy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_first_scan_complete_ ? true : first_scan_delayed_;
}

bool WifiDataProviderChromeOs::GetData(WifiData* data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

void WifiDataProviderChromeOs::ScheduleNextScan(int interval) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(started_);
  if (!NetworkHandler::IsInitialized()) {
    LOG(ERROR) << "ScheduleNextScan called with uninitialized NetworkHandler";
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WifiDataProviderChromeOs::DoWifiScanTask,
                     weak_factory_.GetWeakPtr()),
      base::Milliseconds(interval));
}

void WifiDataProviderChromeOs::DoWifiScanTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!started_) {
    return;
  }
  NetworkHandler::Get()->task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&GetWifiData),
      base::BindOnce(&WifiDataProviderChromeOs::OnWifiScanTaskComplete,
                     weak_factory_.GetWeakPtr()));
}

void WifiDataProviderChromeOs::OnWifiScanTaskComplete(
    std::optional<WifiData> wifi_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!wifi_data) {
    // Schedule next scan if started (StopDataProvider could have been called
    // in between DoWifiScanTask and this method).
    if (started_) {
      ScheduleNextScan(WifiPollingPolicy::Get()->NoWifiInterval());
    }
    return;
  }
  bool update_available = wifi_data_.DiffersSignificantly(*wifi_data);
  wifi_data_ = std::move(*wifi_data);
  // Schedule next scan if started (StopDataProvider could have been called
  // in between DoWifiScanTask and this method).
  if (started_) {
    WifiPollingPolicy::Get()->UpdatePollingInterval(update_available);
    ScheduleNextScan(WifiPollingPolicy::Get()->PollingInterval());
  }

  if (update_available || !is_first_scan_complete_) {
    is_first_scan_complete_ = true;
    RunCallbacks();
  }
}

void WifiDataProviderChromeOs::ScheduleStop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(started_);
  started_ = false;
}

void WifiDataProviderChromeOs::ScheduleStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

std::optional<WifiData> WifiDataProviderChromeOs::GetWifiDataForTesting() {
  return GetWifiData();
}

// static
WifiDataProvider* WifiDataProviderHandle::DefaultFactoryFunction() {
  return new WifiDataProviderChromeOs();
}

}  // namespace device
