// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_GEOLOCATION_MANAGER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_GEOLOCATION_MANAGER_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "build/build_config.h"
#include "build/buildflag.h"

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_CHROMEOS)

#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"
#include "services/device/public/cpp/geolocation/system_geolocation_source.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "services/device/public/mojom/geoposition.mojom.h"
#endif

namespace device {

// This class provides access to location information on the supported OSs.
class COMPONENT_EXPORT(GEOLOCATION) GeolocationManager {
 public:
  // Retrieves the global instance of the Geolocation Manager.
  static GeolocationManager* GetInstance();
  // Sets the global instance of the Geolocation Manager.
  static void SetInstance(std::unique_ptr<GeolocationManager> manager);

  void TrackGeolocationAttempted(const std::string& app_name = "");
  void TrackGeolocationRelinquished(const std::string& app_name = "");

#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_CHROMEOS)
// Default empty implementation of Geolocation Manager. It is used on operation
// systems for which we don't support system-level geolocation. A separate class
// (as opposed to nullptr) makes sure no unsupported calls are made in such
// context.
};  // class GeolocationManager

#else

  class PermissionObserver : public base::CheckedObserver {
   public:
    virtual void OnSystemPermissionUpdated(
        LocationSystemPermissionStatus new_status) = 0;
  };

  using PermissionObserverList =
      base::ObserverListThreadSafe<PermissionObserver>;

#if BUILDFLAG(IS_APPLE)
  class PositionObserver : public base::CheckedObserver {
   public:
    virtual void OnPositionUpdated(const mojom::Geoposition& position) = 0;
    virtual void OnPositionError(const mojom::GeopositionError& error) = 0;
  };

  using PositionObserverList = base::ObserverListThreadSafe<PositionObserver>;
#endif

  explicit GeolocationManager(
      std::unique_ptr<SystemGeolocationSource> system_geolocation_source);
  GeolocationManager(const GeolocationManager&) = delete;
  GeolocationManager& operator=(const GeolocationManager&) = delete;
  virtual ~GeolocationManager();

  // Synchronously retrieves the current system permission status.
  LocationSystemPermissionStatus GetSystemPermission() const;

  // Adds a permission observer.
  void AddObserver(PermissionObserver* observer);
  // Removes a permission observer.
  void RemoveObserver(PermissionObserver* observer);
  // Returns the list of permission observers.
  scoped_refptr<PermissionObserverList> GetObserverList() const;

#if BUILDFLAG(IS_APPLE)
  // Starts the system level process for watching position updates. These
  // updates will trigger a call to and observers in the |position_observers_|
  // list. Upon call the |position_observers_| will be notified of the current
  // position.
  void StartWatchingPosition(bool high_accuracy);
  // Stops the system level process for watching position updates. Observers
  // in the |position_observers_| list will stop receiving updates until
  // StartWatchingPosition is called again.
  void StopWatchingPosition();

  // Returns the list of position observers.
  scoped_refptr<PositionObserverList> GetPositionObserverList() const;
  // Returns the last position
  const mojom::GeopositionResult* GetLastPosition() const;
#endif

 protected:
  SystemGeolocationSource& SystemGeolocationSourceForTest();

 private:
  void UpdateSystemPermission(LocationSystemPermissionStatus status);
  void NotifyPermissionObservers();
#if BUILDFLAG(IS_APPLE)
  void NotifyPositionObservers(mojom::GeopositionResultPtr result);
#endif

  // Using scoped_refptr so objects can hold a reference and ensure this list
  // is not destroyed on shutdown before it had a chance to remove itself from
  // the list
  std::unique_ptr<SystemGeolocationSource> system_geolocation_source_;
  scoped_refptr<PermissionObserverList> observers_;
  LocationSystemPermissionStatus permission_cache_ =
      LocationSystemPermissionStatus::kNotDetermined;

#if BUILDFLAG(IS_APPLE)
  mojom::GeopositionResultPtr last_result_;
  // Using scoped_refptr so objects can hold a reference and ensure this list
  // is not destroyed on shutdown before it had a chance to remove itself from
  // the list
  scoped_refptr<PositionObserverList> position_observers_;
#endif

  base::WeakPtrFactory<GeolocationManager> weak_factory_{this};
};

#endif

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_GEOLOCATION_MANAGER_H_
