// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_CORE_LOCATION_PROVIDER_H_
#define SERVICES_DEVICE_GEOLOCATION_CORE_LOCATION_PROVIDER_H_

#import <CoreLocation/CoreLocation.h>

#include "base/mac/scoped_nsobject.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/device/public/mojom/geoposition.mojom.h"

@class LocationDelegate;

namespace device {

// Location provider for macOS using the platform's Core Location API.
class CoreLocationProvider : public LocationProvider {
 public:
  CoreLocationProvider();
  ~CoreLocationProvider() override;

  // LocationProvider implementation.
  void SetUpdateCallback(
      const LocationProviderUpdateCallback& callback) override;
  void StartProvider(bool high_accuracy) override;
  void StopProvider() override;
  const mojom::Geoposition& GetPosition() override;
  void OnPermissionGranted() override;

  void SystemLocationPermissionGranted();
  void SystemLocationPermissionDenied();
  void DidUpdatePosition(CLLocation* location);
  void SetManagerForTesting(CLLocationManager* location_manager);

 private:
  base::scoped_nsobject<CLLocationManager> location_manager_;
  base::scoped_nsobject<LocationDelegate> delegate_;
  mojom::Geoposition last_position_;
  LocationProviderUpdateCallback callback_;
  bool has_permission_ = false;
  bool provider_start_attemped_ = false;
  base::WeakPtrFactory<CoreLocationProvider> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_LOCATION_PROVIDER_MAC_H_
