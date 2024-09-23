// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_CORE_LOCATION_PROVIDER_H_
#define SERVICES_DEVICE_GEOLOCATION_CORE_LOCATION_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/device/public/cpp/geolocation/system_geolocation_source.h"
#include "services/device/public/mojom/geolocation_internals.mojom.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace device {

// Location provider for macOS using the platform's Core Location API.
class CoreLocationProvider
    : public LocationProvider,
      public SystemGeolocationSource::PositionObserver {
 public:
  explicit CoreLocationProvider(
      SystemGeolocationSource& system_geolocation_source);
  CoreLocationProvider(const CoreLocationProvider&) = delete;
  CoreLocationProvider& operator=(const CoreLocationProvider&) = delete;
  ~CoreLocationProvider() override;

  // LocationProvider implementation.
  void FillDiagnostics(mojom::GeolocationDiagnostics& diagnostics) override;
  void SetUpdateCallback(
      const LocationProviderUpdateCallback& callback) override;
  void StartProvider(bool high_accuracy) override;
  void StopProvider() override;
  const mojom::GeopositionResult* GetPosition() override;
  void OnPermissionGranted() override;

 private:

  // SystemGeolocationSource::PositionObserver implementation.
  void OnPositionUpdated(const mojom::Geoposition& position) override;
  void OnPositionError(const mojom::GeopositionError& error) override;

  mojom::GeopositionResultPtr last_result_;
  LocationProviderUpdateCallback callback_;
  bool is_started_ = false;
  bool high_accuracy_ = false;

  // Currently on macOS, GeolocationSystemPermissionManager and its
  // SystemGeolocationSource are designed to persist through program exit. This
  // allows safe use of a raw_ref since we're guaranteed the object remains
  // valid.
  const base::raw_ref<SystemGeolocationSource> system_geolocation_source_;
  base::WeakPtrFactory<CoreLocationProvider> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_CORE_LOCATION_PROVIDER_H_
