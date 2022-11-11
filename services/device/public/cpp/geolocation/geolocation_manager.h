// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_GEOLOCATION_MANAGER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_GEOLOCATION_MANAGER_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace device {

#if BUILDFLAG(IS_MAC)

// This class is owned by the browser process and keeps track of the macOS
// location permissions for the browser.
class COMPONENT_EXPORT(GEOLOCATION) GeolocationManager {
 public:
  class PermissionObserver : public base::CheckedObserver {
   public:
    virtual void OnSystemPermissionUpdated(
        LocationSystemPermissionStatus new_status) = 0;
  };
  class PositionObserver : public base::CheckedObserver {
   public:
    virtual void OnPositionUpdated(const mojom::Geoposition& position) = 0;
  };

  using PermissionObserverList =
      base::ObserverListThreadSafe<PermissionObserver>;
  using PositionObserverList = base::ObserverListThreadSafe<PositionObserver>;

  GeolocationManager();
  GeolocationManager(const GeolocationManager&) = delete;
  GeolocationManager& operator=(const GeolocationManager&) = delete;
  virtual ~GeolocationManager();

  // Synchronously retrieves the current system permission status.
  virtual LocationSystemPermissionStatus GetSystemPermission() const = 0;
  // Starts the system level process for watching position updates. These
  // updates will trigger a call to and observers in the |position_observers_|
  // list. Upon call the |position_observers_| will be notified of the current
  // position.
  virtual void StartWatchingPosition(bool high_accuracy) = 0;
  // Stops the system level process for watching position updates. Observers
  // in the |position_observers_| list will stop receiving updates until
  // StartWatchingPosition is called again.
  virtual void StopWatchingPosition() = 0;

  void AddObserver(PermissionObserver* observer);
  void RemoveObserver(PermissionObserver* observer);
  scoped_refptr<PermissionObserverList> GetObserverList() const;
  scoped_refptr<PositionObserverList> GetPositionObserverList() const;
  mojom::Geoposition GetLastPosition() const;

 protected:
  void NotifyPermissionObservers(LocationSystemPermissionStatus status);
  void NotifyPositionObservers(const mojom::Geoposition& position);

 private:
  mojom::Geoposition last_position_;
  // Using scoped_refptr so objects can hold a reference and ensure this list
  // is not destroyed on shutdown before it had a chance to remove itself from
  // the list
  scoped_refptr<PermissionObserverList> observers_;
  scoped_refptr<PositionObserverList> position_observers_;
};

#else
class COMPONENT_EXPORT(GEOLOCATION) GeolocationManager {};

#endif

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_GEOLOCATION_MANAGER_H_
