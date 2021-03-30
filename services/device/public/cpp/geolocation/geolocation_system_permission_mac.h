// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_GEOLOCATION_SYSTEM_PERMISSION_MAC_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_GEOLOCATION_SYSTEM_PERMISSION_MAC_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_threadsafe.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"

namespace device {

// This class is owned by the browser process and keeps track of the macOS
// location permissions for the browser.
class COMPONENT_EXPORT(GEOLOCATION) GeolocationSystemPermissionManager {
 public:
  class GeolocationPermissionObserver : public base::CheckedObserver {
   public:
    virtual void OnSystemPermissionUpdate(
        LocationSystemPermissionStatus new_status) = 0;
  };

  using ObserverList =
      base::ObserverListThreadSafe<GeolocationPermissionObserver>;

  GeolocationSystemPermissionManager();
  virtual ~GeolocationSystemPermissionManager();
  static std::unique_ptr<GeolocationSystemPermissionManager> Create();
  virtual LocationSystemPermissionStatus GetSystemPermission() = 0;
  void AddObserver(GeolocationPermissionObserver* observer);
  void RemoveObserver(GeolocationPermissionObserver* observer);
  scoped_refptr<ObserverList> GetObserverList();

 protected:
  void NotifyObservers(LocationSystemPermissionStatus status);

 private:
  scoped_refptr<ObserverList> observers_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_GEOLOCATION_SYSTEM_PERMISSION_MAC_H_
