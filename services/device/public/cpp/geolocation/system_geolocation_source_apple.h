// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_SYSTEM_GEOLOCATION_SOURCE_APPLE_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_SYSTEM_GEOLOCATION_SOURCE_APPLE_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/network_change_notifier.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"
#include "services/device/public/cpp/geolocation/system_geolocation_source.h"

@class CLLocationManager;
@class LocationManagerDelegate;

namespace device {

class COMPONENT_EXPORT(GEOLOCATION) SystemGeolocationSourceApple
    : public SystemGeolocationSource,
      public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  static std::unique_ptr<GeolocationSystemPermissionManager>
  CreateGeolocationSystemPermissionManager();

  // Checks if Wi-Fi is currently enabled on the device.
  static bool IsWifiEnabled();

  // Sets the `mock_wifi_status_` for testing purposes.
  static void SetWifiStatusForTesting(bool mock_wifi_status) {
    mock_wifi_status_ = mock_wifi_status;
  }

  // The maximum time to wait for a network status change event before
  // triggering a timeout.
  static constexpr unsigned int kNetworkChangeTimeoutMs = 1000;

  SystemGeolocationSourceApple();
  ~SystemGeolocationSourceApple() override;

  // SystemGeolocationSource implementation:
  void RegisterPermissionUpdateCallback(
      PermissionUpdateCallback callback) override;

  // To be called from the macOS backend via callback when the permission is
  // updated
  void PermissionUpdated();

  // To be called from the macOS backend via callback when the position is
  // updated
  void PositionUpdated(const mojom::Geoposition& position);
  void PositionError(const mojom::GeopositionError& error);

  void AddPositionUpdateObserver(PositionObserver* observer) override;
  void RemovePositionUpdateObserver(PositionObserver* observer) override;

  // `StartWatchingPosition` and `StopWatchingPosition` will be called from
  // `CoreLocationProvider` on geolocation thread to start / stop watching
  // position.
  void StartWatchingPosition(bool high_accuracy) override;
  void StopWatchingPosition() override;

  void RequestPermission() override;

  void OpenSystemPermissionSetting() override;

  // Actual implementations for Start/Stop-WatchingPosition.
  // These methods are called on the main thread to ensure thread-safety.
  void StartWatchingPositionInternal(bool high_accuracy);
  void StopWatchingPositionInternal();

  // net::NetworkChangeNotifier::NetworkChangeObserver implementation.
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // Indicates whether Wi-Fi was enabled when the last position watch was
  // started.
  bool WasWifiEnabled() { return was_wifi_enabled_; }

  // Start the timer that wait for network status change event with a connection
  // type other than `net::NetworkChangeNotifier::CONNECTION_NONE`.
  void StartNetworkChangedTimer();

  // Invoked when the `network_changed_timer_` expires, indicating a timeout
  // while waiting for a network status change event.
  void OnNetworkChangedTimeout();

  // Sets the `location_manager_` for testing purposes.
  void SetLocationManagerForTesting(CLLocationManager* manager) {
    location_manager_ = manager;
  }

  // Gets the location manager delegate for testing.
  LocationManagerDelegate* GetDelegateForTesting() { return delegate_; }

 private:
  friend class SystemGeolocationSourceAppleTest;
  // The enum represents the possible outcomes of a session (from starting to
  // stopping provider).
  enum class CoreLocationSessionResult {
    kSuccess = 0,
    kCoreLocationError = 1,
    kWifiDisabled = 2,
  };

  // Mock wifi status only used for testing.
  static std::optional<bool> mock_wifi_status_;
  LocationSystemPermissionStatus GetSystemPermission() const;
  LocationManagerDelegate* __strong delegate_;
  CLLocationManager* __strong location_manager_;
  PermissionUpdateCallback permission_update_callback_;
  scoped_refptr<PositionObserverList> position_observers_;
  // A reusable timer to detect timeouts when waiting for network status change
  // events. When the timer expires, `OnNetworkChangedTimeout` is invoked.
  base::RetainingOneShotTimer network_changed_timer_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  // Caches the initial Wi-Fi status at the beginning of watching position.
  bool was_wifi_enabled_ = false;
  // Indicates whether any position updates have been received.
  bool position_received_ = false;
  // Stores the result of the CoreLocation session. This will hold the first
  // error encountered, or be empty to indicate a successful session with no
  // errors.
  std::optional<CoreLocationSessionResult> session_result_;
  // Time when position watching started. Used to calculate the time to first
  // position is updated.
  base::TimeTicks watch_start_time_;
  base::WeakPtrFactory<SystemGeolocationSourceApple> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_SYSTEM_GEOLOCATION_SOURCE_APPLE_H_
