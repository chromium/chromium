// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_SYSTEM_GEOLOCATION_SOURCE_MAC_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_SYSTEM_GEOLOCATION_SOURCE_MAC_H_

#include "base/memory/weak_ptr.h"
#include "services/device/public/cpp/geolocation/geolocation_manager.h"
#include "services/device/public/cpp/geolocation/system_geolocation_source.h"

@class GeolocationManagerDelegate;
@class CLLocationManager;

namespace device {

class COMPONENT_EXPORT(GEOLOCATION) SystemGeolocationSourceMac
    : public SystemGeolocationSource {
 public:
  static std::unique_ptr<GeolocationManager> CreateGeolocationManagerOnMac();

  SystemGeolocationSourceMac();
  ~SystemGeolocationSourceMac() override;

  // SystemGeolocationSource implementation:
  void RegisterPermissionUpdateCallback(
      PermissionUpdateCallback callback) override;
  void RegisterPositionUpdateCallback(PositionUpdateCallback callback) override;

  // To be called from the macOS backend via callback when the permission is
  // updated
  void PermissionUpdated();

  // To be called from the macOS backend via callback when the position is
  // updated
  void PositionUpdated(const mojom::Geoposition& position);
  void PositionError(const mojom::GeopositionError& error);

  void StartWatchingPosition(bool high_accuracy) override;
  void StopWatchingPosition() override;
  void RequestPermission() override;

  // Calls requestWhenInUseAuthorization from CLLocationManager.
  void TrackGeolocationAttempted() override;

 private:
  LocationSystemPermissionStatus GetSystemPermission() const;

  GeolocationManagerDelegate* __strong delegate_;
  CLLocationManager* __strong location_manager_;
  SEQUENCE_CHECKER(sequence_checker_);
  PermissionUpdateCallback permission_update_callback_;
  PositionUpdateCallback position_update_callback_;
  base::WeakPtrFactory<SystemGeolocationSourceMac> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_SYSTEM_GEOLOCATION_SOURCE_MAC_H_
