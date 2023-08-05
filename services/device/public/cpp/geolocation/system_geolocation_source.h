// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_SYSTEM_GEOLOCATION_SOURCE_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_SYSTEM_GEOLOCATION_SOURCE_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "location_system_permission_status.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace device {

#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_CHROMEOS)
#error This file should be compiled only on Apple and ChromeOS\
  (i.e. platforms where we support system-based geolocation permissions)
#endif

// This interface is used by the Geolocation Manager. It encapsulates the
// OS-specific logic that provides the geolocation data on the supported OSs. It
// is supposed to be injected into the Geolocation Manager so that the
// GeolocationManager implementation can be OS-agnostic, delegating all
// OS-specific details to the SystemGeolocationSource.
class COMPONENT_EXPORT(GEOLOCATION) SystemGeolocationSource {
 public:
  using PermissionUpdateCallback =
      base::RepeatingCallback<void(LocationSystemPermissionStatus)>;

#if BUILDFLAG(IS_APPLE)
  using PositionUpdateCallback =
      base::RepeatingCallback<void(mojom::GeopositionResultPtr)>;
#endif

  virtual ~SystemGeolocationSource() = default;

  // This method accepts a callback. The callback is to be called
  // once to provide the current value and then again always when the permission
  // changes in the OS. The first call may be synchronous or asynchronous.
  // The subsequent calls are asynchronous.
  virtual void RegisterPermissionUpdateCallback(
      PermissionUpdateCallback callback) = 0;

  // Informs system that some page wants to use geolocation. This function may
  // be implemented if the OS specific implementation requires it.
  virtual void TrackGeolocationAttempted() {}
  // Informs that some page does not need to use geolocation any more. This
  // function should be called only if the intention to use geolocation was
  // signalled for the same page using TrackGeolocationAttempted(). This
  // function may be implemented if the OS specific implementation requires it.
  virtual void TrackGeolocationRelinquished() {}

#if BUILDFLAG(IS_APPLE)
  // This method accepts a callback. The callback is called whenever a new
  // position estimate is available.
  virtual void RegisterPositionUpdateCallback(
      PositionUpdateCallback callback) = 0;

  // Starts the system level process for watching position updates. These
  // updates will trigger a call to and observers in the |position_observers_|
  // list. Upon call the |position_observers_| will be notified of the current
  // position.
  virtual void StartWatchingPosition(bool high_accuracy) = 0;

  // Stops the system level process for watching position updates. Observers
  // in the |position_observers_| list will stop receiving updates until
  // StartWatchingPosition is called again.
  virtual void StopWatchingPosition() = 0;

  // Requests system level permission to use geolocation. This may cause a
  // permission dialog to be displayed. The permission update callback is called
  // if the permission state changes.
  virtual void RequestPermission() = 0;
#endif
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_SYSTEM_GEOLOCATION_SOURCE_H_
