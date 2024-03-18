// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_SYSTEM_GEOLOCATION_SOURCE_APPLE_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_SYSTEM_GEOLOCATION_SOURCE_APPLE_H_

#include "base/memory/weak_ptr.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"
#include "services/device/public/cpp/geolocation/system_geolocation_source.h"

@class GeolocationSystemPermissionManagerDelegate;
@class CLLocationManager;

namespace device {

class COMPONENT_EXPORT(GEOLOCATION) SystemGeolocationSourceApple
    : public SystemGeolocationSource {
 public:
  static std::unique_ptr<GeolocationSystemPermissionManager>
  CreateGeolocationSystemPermissionManager();

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
  void StartWatchingPosition(bool high_accuracy) override;
  void StopWatchingPosition() override;
  void RequestPermission() override;

  void OpenSystemPermissionSetting() override;

 private:
  LocationSystemPermissionStatus GetSystemPermission() const;
  GeolocationSystemPermissionManagerDelegate* __strong delegate_;
  CLLocationManager* __strong location_manager_;
  SEQUENCE_CHECKER(sequence_checker_);
  PermissionUpdateCallback permission_update_callback_;
  scoped_refptr<PositionObserverList> position_observers_;
  base::WeakPtrFactory<SystemGeolocationSourceApple> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_SYSTEM_GEOLOCATION_SOURCE_APPLE_H_
