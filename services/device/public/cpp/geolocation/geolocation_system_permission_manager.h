// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_GEOLOCATION_SYSTEM_PERMISSION_MANAGER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_GEOLOCATION_SYSTEM_PERMISSION_MANAGER_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "services/device/public/cpp/geolocation/buildflags.h"

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)

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
class COMPONENT_EXPORT(GEOLOCATION) GeolocationSystemPermissionManager {
 public:
  // Retrieves the global instance of the GeolocationSystemPermissionManager.
  static GeolocationSystemPermissionManager* GetInstance();
  // Sets the global instance of the GeolocationSystemPermissionManager.
  static void SetInstance(
      std::unique_ptr<GeolocationSystemPermissionManager> manager);

  void RequestSystemPermission();
  // Opens appropriate system preferences/setting page.
  void OpenSystemPermissionSetting();

#if !BUILDFLAG(IS_APPLE) && \
    !BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  // Default empty implementation of GeolocationSystemPermissionManager.
  // It is used on operation systems for which we don't support system-level
  // geolocation. A separate class (as opposed to nullptr) makes sure no
  // unsupported calls are made in such context.
};  // class GeolocationSystemPermissionManager

#else

  class PermissionObserver : public base::CheckedObserver {
   public:
    virtual void OnSystemPermissionUpdated(
        LocationSystemPermissionStatus new_status) = 0;
  };

  using PermissionObserverList =
      base::ObserverListThreadSafe<PermissionObserver>;

  explicit GeolocationSystemPermissionManager(
      std::unique_ptr<SystemGeolocationSource> system_geolocation_source);
  GeolocationSystemPermissionManager(
      const GeolocationSystemPermissionManager&) = delete;
  GeolocationSystemPermissionManager& operator=(
      const GeolocationSystemPermissionManager&) = delete;
  virtual ~GeolocationSystemPermissionManager();

  // Synchronously retrieves the current system permission status.
  LocationSystemPermissionStatus GetSystemPermission() const;

  // Adds a permission observer.
  void AddObserver(PermissionObserver* observer);
  // Removes a permission observer.
  void RemoveObserver(PermissionObserver* observer);
  // Returns the list of permission observers.
  scoped_refptr<PermissionObserverList> GetObserverList() const;

#if BUILDFLAG(IS_APPLE)
  // On macOS, the same CLLocationManager object needs to be shared across
  // permission and location updates or permission state might be out of sync
  // when there is a pending app update. Refer to crbug.com/1143807 for more
  // info.
  SystemGeolocationSource& GetSystemGeolocationSource() {
    return *system_geolocation_source_;
  }
#endif

  SystemGeolocationSource& SystemGeolocationSourceForTest();

 private:
  void UpdateSystemPermission(LocationSystemPermissionStatus status);
  void NotifyPermissionObservers();

  std::unique_ptr<SystemGeolocationSource> system_geolocation_source_;

  // Using scoped_refptr so objects can hold a reference and ensure this list
  // is not destroyed on shutdown before it had a chance to remove itself from
  // the list
  scoped_refptr<PermissionObserverList> observers_;
  LocationSystemPermissionStatus permission_cache_ =
      LocationSystemPermissionStatus::kNotDetermined;
  base::WeakPtrFactory<GeolocationSystemPermissionManager> weak_factory_{this};
};

#endif

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_GEOLOCATION_SYSTEM_PERMISSION_MANAGER_H_
