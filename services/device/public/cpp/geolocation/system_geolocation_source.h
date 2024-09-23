// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_SYSTEM_GEOLOCATION_SOURCE_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_SYSTEM_GEOLOCATION_SOURCE_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "location_system_permission_status.h"
#include "services/device/public/cpp/geolocation/buildflags.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace device {

#if !BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
#error This file should be compiled only on Apple, ChromeOS, or Windows\
  (i.e. platforms where we support system-based geolocation permissions)
#endif

// This interface is used by the GeolocationSystemPermissionManager (GSPM). It
// encapsulates the OS-specific logic that provides the geolocation data on the
// supported OSs. It is supposed to be injected into the GSPM so that the GSPM
// implementation can be OS-agnostic, delegating all OS-specific details to the
// SystemGeolocationSource.
class COMPONENT_EXPORT(GEOLOCATION) SystemGeolocationSource {
 public:
  using PermissionUpdateCallback =
      base::RepeatingCallback<void(LocationSystemPermissionStatus)>;

#if BUILDFLAG(IS_APPLE)
  class PositionObserver : public base::CheckedObserver {
   public:
    virtual void OnPositionUpdated(const mojom::Geoposition& position) = 0;
    virtual void OnPositionError(const mojom::GeopositionError& position) = 0;
  };
  using PositionObserverList = base::ObserverListThreadSafe<PositionObserver>;
#endif

  virtual ~SystemGeolocationSource() = default;

  // This method accepts a callback. The callback is to be called
  // once to provide the current value and then again always when the permission
  // changes in the OS. The first call may be synchronous or asynchronous.
  // The subsequent calls are asynchronous.
  virtual void RegisterPermissionUpdateCallback(
      PermissionUpdateCallback callback) = 0;

  // Opens system specific permission settings page (if available on this
  // platform).
  virtual void OpenSystemPermissionSetting() {}

#if BUILDFLAG(IS_APPLE)
  // Allows position observers to register/unregister themselves for updates.
  virtual void AddPositionUpdateObserver(PositionObserver* observer) = 0;
  virtual void RemovePositionUpdateObserver(PositionObserver* observer) = 0;

  // Starts the system level process for watching position updates. These
  // updates will trigger a call to and observers in the |position_observers_|
  // list. Upon call the |position_observers_| will be notified of the current
  // position.
  virtual void StartWatchingPosition(bool high_accuracy) = 0;

  // Stops the system level process for watching position updates. Observers
  // in the |position_observers_| list will stop receiving updates until
  // StartWatchingPosition is called again.
  virtual void StopWatchingPosition() = 0;
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
  // Requests system level permission to use geolocation. This may cause a
  // permission dialog to be displayed. The permission update callback is called
  // if the permission state changes.
  virtual void RequestPermission() = 0;
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_SYSTEM_GEOLOCATION_SOURCE_H_
