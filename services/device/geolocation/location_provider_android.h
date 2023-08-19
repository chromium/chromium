// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_LOCATION_PROVIDER_ANDROID_H_
#define SERVICES_DEVICE_GEOLOCATION_LOCATION_PROVIDER_ANDROID_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/device/public/mojom/geolocation_internals.mojom.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace device {

// Location provider for Android using the platform provider over JNI.
class LocationProviderAndroid : public LocationProvider {
 public:
  LocationProviderAndroid();
  ~LocationProviderAndroid() override;

  // Called by the LocationApiAdapterAndroid.
  void NotifyNewGeoposition(mojom::GeopositionResultPtr result);

  // LocationProvider implementation.
  void FillDiagnostics(mojom::GeolocationDiagnostics& diagnostics) override;
  void SetUpdateCallback(
      const LocationProviderUpdateCallback& callback) override;
  void StartProvider(bool high_accuracy) override;
  void StopProvider() override;
  const mojom::GeopositionResult* GetPosition() override;
  void OnPermissionGranted() override;

 private:
  base::ThreadChecker thread_checker_;

  mojom::GeolocationDiagnostics::ProviderState state_ =
      mojom::GeolocationDiagnostics::ProviderState::kStopped;
  mojom::GeopositionResultPtr last_result_;
  LocationProviderUpdateCallback callback_;

  base::WeakPtrFactory<LocationProviderAndroid> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_LOCATION_PROVIDER_ANDROID_H_
