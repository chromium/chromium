// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/wifi_data_provider_lacros.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/lacros/lacros_service.h"
#include "services/device/geolocation/wifi_data_provider_handle.h"
#include "services/device/geolocation/wifi_polling_policy.h"
#include "services/device/public/mojom/geolocation_internals.mojom.h"

namespace device {

namespace {

// The time periods between successive polls of the wifi data.
constexpr int kDefaultPollingIntervalMilliseconds = 10 * 1000;
constexpr int kNoChangePollingIntervalMilliseconds = 2 * 60 * 1000;
constexpr int kTwoNoChangePollingIntervalMilliseconds = 10 * 60 * 1000;
constexpr int kNoWifiPollingIntervalMilliseconds = 20 * 1000;

std::unique_ptr<WifiPollingPolicy> CreatePollingPolicy() {
  return std::make_unique<GenericWifiPollingPolicy<
      kDefaultPollingIntervalMilliseconds, kNoChangePollingIntervalMilliseconds,
      kTwoNoChangePollingIntervalMilliseconds,
      kNoWifiPollingIntervalMilliseconds>>();
}

void PopulateWifiData(
    const std::vector<crosapi::mojom::AccessPointDataPtr>& access_points,
    WifiData& wifi_data) {
  for (const auto& access_point : access_points) {
    mojom::AccessPointData ap_data;
    ap_data.mac_address = base::UTF16ToUTF8(access_point->mac_address);
    ap_data.radio_signal_strength = access_point->radio_signal_strength;
    ap_data.channel = access_point->channel;
    ap_data.signal_to_noise = access_point->signal_to_noise;
    wifi_data.access_point_data.insert(ap_data);
  }
}

// crosapi::GeolocationService is not available if either the LacrosService is
// not available or if the current version of ash is not new enough to support
// the GeolocationService.
bool IsGeolocationServiceAvailable() {
  if (!chromeos::LacrosService::Get())
    return false;
  const int crosapiVersion =
      chromeos::LacrosService::Get()
          ->GetInterfaceVersion<crosapi::mojom::Crosapi>();
  const int minRequiredVersion = static_cast<int>(
      crosapi::mojom::Crosapi::kBindGeolocationServiceMinVersion);
  return crosapiVersion >= minRequiredVersion;
}

}  // namespace

WifiDataProviderLacros::WifiDataProviderLacros() = default;

WifiDataProviderLacros::~WifiDataProviderLacros() = default;

void WifiDataProviderLacros::StartDataProvider() {
  DCHECK(CalledOnClientThread());
  DCHECK(!started_);

  // Do not start the provider if the GeolocationService is not available.
  if (!IsGeolocationServiceAvailable()) {
    is_first_scan_complete_ = true;
    return;
  }

  if (!WifiPollingPolicy::IsInitialized())
    WifiPollingPolicy::Initialize(CreatePollingPolicy());
  DCHECK(WifiPollingPolicy::IsInitialized());

  started_ = true;
  int delay_interval_ms = WifiPollingPolicy::Get()->InitialInterval();
  ScheduleNextScan(delay_interval_ms);
  first_scan_delayed_ = (delay_interval_ms > 0);
}

void WifiDataProviderLacros::StopDataProvider() {
  DCHECK(CalledOnClientThread());
  if (started_) {
    wifi_scan_timer_.Stop();
    started_ = false;
  }
}

bool WifiDataProviderLacros::DelayedByPolicy() {
  DCHECK(CalledOnClientThread());
  return is_first_scan_complete_ ? true : first_scan_delayed_;
}

bool WifiDataProviderLacros::GetData(WifiData* data) {
  DCHECK(CalledOnClientThread());
  DCHECK(data);
  *data = wifi_data_;
  return is_first_scan_complete_;
}

// There is currently no reason to force a rescan on ChromeOS so this has not
// been implemented.
void WifiDataProviderLacros::ForceRescan() {}

void WifiDataProviderLacros::DidWifiScanTaskForTesting(
    bool service_initialized,
    bool data_available,
    base::TimeDelta time_since_last_updated,
    std::vector<crosapi::mojom::AccessPointDataPtr> access_points) {
  DidWifiScanTask(service_initialized, data_available, time_since_last_updated,
                  std::move(access_points));
}

void WifiDataProviderLacros::ScheduleNextScan(int interval_ms) {
  // Do not schedule a scan if the GeolocationService is not available or if not
  // `started_`. This could occur if DoWifiScanTask() is called back after the
  // provider has been stopped.
  if (!IsGeolocationServiceAvailable() || !started_)
    return;

  base::TimeDelta interval = base::Milliseconds(interval_ms);
  if (!wifi_scan_timer_.IsRunning() ||
      interval < wifi_scan_timer_.GetCurrentDelay()) {
    wifi_scan_timer_.Start(
        FROM_HERE, interval,
        base::BindRepeating(&WifiDataProviderLacros::DoWifiScanTask,
                            base::Unretained(this)));
  }
}

void WifiDataProviderLacros::DoWifiScanTask() {
  DCHECK(started_);
  DCHECK(IsGeolocationServiceAvailable());

  if (!geolocation_service_.is_bound()) {
    chromeos::LacrosService::Get()->BindGeolocationService(
        geolocation_service_.BindNewPipeAndPassReceiver());
  }

  geolocation_service_->GetWifiAccessPoints(base::BindOnce(
      &WifiDataProviderLacros::DidWifiScanTask, weak_factory_.GetWeakPtr()));
}

void WifiDataProviderLacros::DidWifiScanTask(
    bool service_initialized,
    bool data_available,
    base::TimeDelta time_since_last_updated,
    std::vector<crosapi::mojom::AccessPointDataPtr> access_points) {
  if (!service_initialized) {
    LOG(ERROR) << "DoWifiScanTask() called with uninitialized NetworkHandler";
    return;
  }

  // If the age is significantly longer than our long polling time, assume the
  // data is stale and trigger a faster update.
  const bool is_data_stale =
      time_since_last_updated >
      base::Milliseconds(kTwoNoChangePollingIntervalMilliseconds * 2);
  if (!data_available || is_data_stale) {
    ScheduleNextScan(WifiPollingPolicy::Get()->NoWifiInterval());
    return;
  }

  WifiData new_data;
  PopulateWifiData(access_points, new_data);
  const bool update_available = wifi_data_.DiffersSignificantly(new_data);
  wifi_data_ = new_data;
  WifiPollingPolicy::Get()->UpdatePollingInterval(update_available);
  ScheduleNextScan(WifiPollingPolicy::Get()->PollingInterval());

  if (update_available || !is_first_scan_complete_) {
    is_first_scan_complete_ = true;
    RunCallbacks();
  }
}

// static
WifiDataProvider* WifiDataProviderHandle::DefaultFactoryFunction() {
  return new WifiDataProviderLacros();
}

}  // namespace device
