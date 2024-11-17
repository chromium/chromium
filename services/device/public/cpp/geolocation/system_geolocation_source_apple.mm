// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/geolocation/system_geolocation_source_apple.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/mac/mac_util.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/public/cpp/geolocation/location_manager_delegate.h"

#if BUILDFLAG(IS_MAC)
#import <CoreWLAN/CoreWLAN.h>
#endif

namespace device {

std::optional<bool> SystemGeolocationSourceApple::mock_wifi_status_;

// static
bool SystemGeolocationSourceApple::IsWifiEnabled() {
  if (mock_wifi_status_.has_value()) {
    return *mock_wifi_status_;
  }
#if BUILDFLAG(IS_MAC)
  CWWiFiClient* wifi_client = [CWWiFiClient sharedWiFiClient];
  CWInterface* interface = wifi_client.interface;
  return (interface && interface.powerOn);
#else
  return true;
#endif
}

SystemGeolocationSourceApple::SystemGeolocationSourceApple()
    : location_manager_([[CLLocationManager alloc] init]),
      permission_update_callback_(base::DoNothing()),
      position_observers_(base::MakeRefCounted<PositionObserverList>()),
      main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  delegate_ = [[LocationManagerDelegate alloc]
      initWithManager:weak_ptr_factory_.GetWeakPtr()];
  location_manager_.delegate = delegate_;
}

SystemGeolocationSourceApple::~SystemGeolocationSourceApple() = default;

// static
std::unique_ptr<GeolocationSystemPermissionManager>
SystemGeolocationSourceApple::CreateGeolocationSystemPermissionManager() {
  return std::make_unique<GeolocationSystemPermissionManager>(
      std::make_unique<SystemGeolocationSourceApple>());
}

void SystemGeolocationSourceApple::RegisterPermissionUpdateCallback(
    PermissionUpdateCallback callback) {
  permission_update_callback_ = callback;
  permission_update_callback_.Run(GetSystemPermission());
}

void SystemGeolocationSourceApple::PermissionUpdated() {
  permission_update_callback_.Run(GetSystemPermission());
}

void SystemGeolocationSourceApple::PositionUpdated(
    const mojom::Geoposition& position) {
  CHECK(main_task_runner_->BelongsToCurrentThread());

  // Record the time to first position update. This is only done once to capture
  // the initial position acquisition time.
  if (!position_received_) {
    const base::TimeDelta time_to_first_position =
        base::TimeTicks::Now() - watch_start_time_;
    UmaHistogramCustomTimes(
        "Geolocation.CoreLocationProvider.TimeToFirstPosition",
        time_to_first_position, base::Milliseconds(1), base::Seconds(10), 100);
    position_received_ = true;
  }

  position_observers_->Notify(FROM_HERE, &PositionObserver::OnPositionUpdated,
                              position);
}

void SystemGeolocationSourceApple::PositionError(
    const mojom::GeopositionError& error) {
  CHECK(main_task_runner_->BelongsToCurrentThread());
  if (!session_result_) {
    session_result_ = CoreLocationSessionResult::kCoreLocationError;
  }
  // If an error reported from `LocationManagerDelegate` when
  // network change timer is running. Stop the timer (which also cancel the
  // pending fallback) and report that error.
  if (network_changed_timer_.IsRunning()) {
    GEOLOCATION_LOG(DEBUG) << "SystemGeolocationSourceApple::PositionError: "
                              "Network status change timer is cancelled.";
    network_changed_timer_.Stop();
    net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  }
  position_observers_->Notify(FROM_HERE, &PositionObserver::OnPositionError,
                              error);
}

void SystemGeolocationSourceApple::StartWatchingPositionInternal(
    bool high_accuracy) {
  CHECK(main_task_runner_->BelongsToCurrentThread());
  watch_start_time_ = base::TimeTicks::Now();
  if (high_accuracy) {
    location_manager_.desiredAccuracy = kCLLocationAccuracyBest;
  } else {
    // Using kCLLocationAccuracyHundredMeters for consistency with Android.
    location_manager_.desiredAccuracy = kCLLocationAccuracyHundredMeters;
  }
  [location_manager_ startUpdatingLocation];
  was_wifi_enabled_ = IsWifiEnabled();
}

void SystemGeolocationSourceApple::StartWatchingPosition(bool high_accuracy) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SystemGeolocationSourceApple::StartWatchingPositionInternal,
          weak_ptr_factory_.GetWeakPtr(), high_accuracy));
}

void SystemGeolocationSourceApple::StopWatchingPositionInternal() {
  CHECK(main_task_runner_->BelongsToCurrentThread());
  [location_manager_ stopUpdatingLocation];
  // If `StopWatchingPosition` is called for any reason, stop the network status
  // event timer (which also cancel the pending fallback).
  if (network_changed_timer_.IsRunning()) {
    GEOLOCATION_LOG(DEBUG)
        << "SystemGeolocationSourceApple::StopWatchingPositionInternal: "
           "Network status change timer is cancelled.";
    network_changed_timer_.Stop();
    net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  }

  // Record the session result if either:
  // 1. An error occurred (session_result_ is set).
  // 2. At least one position update was received (position_received_ is true).
  // This excludes short-lived sessions that start and stop immediately
  // without obtaining any position updates.
  if (session_result_ || position_received_) {
    base::UmaHistogramSparse("Geolocation.CoreLocationProvider.SessionResult",
                             static_cast<int>(session_result_.value_or(
                                 CoreLocationSessionResult::kSuccess)));
  }
  session_result_.reset();
  position_received_ = false;
}

void SystemGeolocationSourceApple::StopWatchingPosition() {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SystemGeolocationSourceApple::StopWatchingPositionInternal,
          weak_ptr_factory_.GetWeakPtr()));
}

LocationSystemPermissionStatus
SystemGeolocationSourceApple::GetSystemPermission() const {
  CHECK(main_task_runner_->BelongsToCurrentThread());
  if (![delegate_ permissionInitialized]) {
    return LocationSystemPermissionStatus::kNotDetermined;
  }

  if ([delegate_ hasPermission]) {
    return LocationSystemPermissionStatus::kAllowed;
  }

  return LocationSystemPermissionStatus::kDenied;
}

void SystemGeolocationSourceApple::OpenSystemPermissionSetting() {
#if BUILDFLAG(IS_MAC)
  base::mac::OpenSystemSettingsPane(
      base::mac::SystemSettingsPane::kPrivacySecurity_LocationServices);
#endif
}

void SystemGeolocationSourceApple::RequestPermission() {
  [location_manager_ requestWhenInUseAuthorization];
}

void SystemGeolocationSourceApple::AddPositionUpdateObserver(
    PositionObserver* observer) {
  position_observers_->AddObserver(observer);
}

void SystemGeolocationSourceApple::RemovePositionUpdateObserver(
    PositionObserver* observer) {
  position_observers_->RemoveObserver(observer);
}

void SystemGeolocationSourceApple::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  CHECK(main_task_runner_->BelongsToCurrentThread());
  // When Wi-Fi is disabled, we initially receive a
  // `net::NetworkChangeNotifier::CONNECTION_NONE` event. Subsequently, after
  // the network stabilizes, another network change event will be triggered
  // (potentially any connection type except
  // `net::NetworkChangeNotifier::CONNECTION_NONE`).
  GEOLOCATION_LOG(DEBUG) << "SystemGeolocationSourceApple::OnNetworkChanged: "
                            "Invoked with connection type = "
                         << static_cast<int>(type);
  if (type != net::NetworkChangeNotifier::CONNECTION_NONE &&
      network_changed_timer_.IsRunning()) {
    GEOLOCATION_LOG(DEBUG)
        << "SystemGeolocationSourceApple::OnNetworkChanged: Network status is "
           "settled, create error position with kWifiDisabled error code to "
           "start fallback mechanism.";
    network_changed_timer_.Stop();
    net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);

    device::mojom::GeopositionError position_error;
    position_error.error_code =
        device::mojom::GeopositionErrorCode::kWifiDisabled;
    position_error.error_message =
        device::mojom::kGeoPositionUnavailableErrorMessage;
    position_error.error_technical =
        "CoreLocationProvider: CoreLocation framework reported a "
        "kWifiDisabled error.";
    session_result_ = CoreLocationSessionResult::kWifiDisabled;
    PositionError(position_error);
  }
}

void SystemGeolocationSourceApple::StartNetworkChangedTimer() {
  CHECK(main_task_runner_->BelongsToCurrentThread());
  if (network_changed_timer_.IsRunning()) {
    GEOLOCATION_LOG(DEBUG)
        << "SystemGeolocationSourceApple::StartNetworkChangedTimer: Network "
           "status change timer is already running, ignore this call.";
    return;
  }
  GEOLOCATION_LOG(DEBUG)
      << "SystemGeolocationSourceApple::StartNetworkChangedTimer: Network "
         "status change timer is started.";

  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);

  // This timer prevents premature fallback initiation if the network state is
  // unstable. Empirically, network change events occur ~600-700ms after Wi-Fi
  // is disabled. A 1-second timeout accommodates potential variations across
  // different machines.
  network_changed_timer_.Start(
      FROM_HERE, base::Milliseconds(kNetworkChangeTimeoutMs), this,
      &SystemGeolocationSourceApple::OnNetworkChangedTimeout);
}

void SystemGeolocationSourceApple::OnNetworkChangedTimeout() {
  CHECK(main_task_runner_->BelongsToCurrentThread());
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  GEOLOCATION_LOG(DEBUG)
      << "SystemGeolocationSourceApple::OnNetworkChangedTimeout: Network "
         "status change timer timed out.";

  device::mojom::GeopositionError position_error;
  position_error.error_code =
      device::mojom::GeopositionErrorCode::kPositionUnavailable;
  position_error.error_message =
      device::mojom::kGeoPositionUnavailableErrorMessage;
  position_error.error_technical =
      "CoreLocationProvider: CoreLocation framework reported a "
      "kCLErrorLocationUnknown failure.";
  PositionError(position_error);
}

}  // namespace device
