// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_LOCATION_PROVIDER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_LOCATION_PROVIDER_H_

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "services/device/public/mojom/geolocation_internals.mojom.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace device {

// The interface for providing location information.
class LocationProvider {
 public:
  virtual ~LocationProvider() {}

  typedef base::RepeatingCallback<void(const LocationProvider*,
                                       mojom::GeopositionResultPtr)>
      LocationProviderUpdateCallback;

  // Populate `diagnostics` with the internal state of this provider.
  virtual void FillDiagnostics(mojom::GeolocationDiagnostics& diagnostics) = 0;

  // This callback will be used to notify when a new Geoposition becomes
  // available.
  virtual void SetUpdateCallback(
      const LocationProviderUpdateCallback& callback) = 0;

  // StartProvider maybe called multiple times, e.g. to alter the
  // |high_accuracy| setting.
  virtual void StartProvider(bool high_accuracy) = 0;

  // Stops the provider from sending more requests.
  // Important: a LocationProvider may be instantiated and StartProvider() may
  // be called before the user has granted permission via OnPermissionGranted().
  // This is to allow underlying providers to warm up, load their internal
  // libraries, etc. No |LocationProviderUpdateCallback| can be run and no
  // network requests can be done until OnPermissionGranted() has been called.
  virtual void StopProvider() = 0;

  // Gets the current best position estimate, or nullptr if no position estimate
  // has been received.
  virtual const mojom::GeopositionResult* GetPosition() = 0;

  // Called everytime permission is granted to a page for using geolocation.
  // This may either be through explicit user action (e.g. responding to the
  // infobar prompt) or inferred from a persisted site permission.
  // Note: See |StartProvider()| for more information.
  virtual void OnPermissionGranted() = 0;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_LOCATION_PROVIDER_H_
