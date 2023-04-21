// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_CORE_LOCATION_PROVIDER_H_
#define SERVICES_DEVICE_GEOLOCATION_CORE_LOCATION_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "services/device/public/cpp/geolocation/geolocation_manager.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace device {

// Location provider for macOS using the platform's Core Location API.
class CoreLocationProvider : public LocationProvider,
                             public GeolocationManager::PermissionObserver,
                             public GeolocationManager::PositionObserver {
 public:
  CoreLocationProvider(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      GeolocationManager* geolocation_manager);
  CoreLocationProvider(const CoreLocationProvider&) = delete;
  CoreLocationProvider& operator=(const CoreLocationProvider&) = delete;
  ~CoreLocationProvider() override;

  // LocationProvider implementation.
  void SetUpdateCallback(
      const LocationProviderUpdateCallback& callback) override;
  void StartProvider(bool high_accuracy) override;
  void StopProvider() override;
  const mojom::GeopositionResult* GetPosition() override;
  void OnPermissionGranted() override;

 private:
  void StartWatching();

  // GeolocationManager::PositionObserver implementation.
  void OnPositionUpdated(const mojom::Geoposition& position) override;
  void OnPositionError(const mojom::GeopositionError& error) override;

  // GeolocationManager::PermissionObserver implementation.
  void OnSystemPermissionUpdated(
      LocationSystemPermissionStatus new_status) override;

  raw_ptr<GeolocationManager> geolocation_manager_;
  // References to the observer lists are kept to ensure their lifetime as the
  // BrowserProcess may destroy its reference on the UI Thread before we
  // destroy this provider.
  scoped_refptr<GeolocationManager::PermissionObserverList>
      permission_observers_;
  scoped_refptr<GeolocationManager::PositionObserverList> position_observers_;
  mojom::GeopositionResultPtr last_result_;
  LocationProviderUpdateCallback callback_;
  bool has_permission_ = false;
  bool provider_start_attemped_ = false;
  bool high_accuracy_ = false;
  base::WeakPtrFactory<CoreLocationProvider> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_CORE_LOCATION_PROVIDER_H_
