// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_GEOLOCATION_MANAGER_IMPL_MAC_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_GEOLOCATION_MANAGER_IMPL_MAC_H_

#include "base/mac/scoped_nsobject.h"
#include "services/device/public/cpp/geolocation/geolocation_manager.h"

@class GeolocationManagerDelegate;
@class CLLocationManager;

namespace device {

class COMPONENT_EXPORT(GEOLOCATION) GeolocationManagerImpl
    : public GeolocationManager {
 public:
  static std::unique_ptr<GeolocationManager> Create();

  GeolocationManagerImpl();
  ~GeolocationManagerImpl() override;

  void PermissionUpdated();
  void PositionUpdated(const mojom::Geoposition& position);

  // GeolocationManager implementation:
  void StartWatchingPosition(bool high_accuracy) override;
  void StopWatchingPosition() override;
  LocationSystemPermissionStatus GetSystemPermission() const override;

 private:
  base::scoped_nsobject<GeolocationManagerDelegate> delegate_;
  base::scoped_nsobject<CLLocationManager> location_manager_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<GeolocationManagerImpl> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_GEOLOCATION_MANAGER_IMPL_MAC_H_
