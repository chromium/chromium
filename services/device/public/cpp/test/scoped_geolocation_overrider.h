// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_SCOPED_GEOLOCATION_OVERRIDER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_SCOPED_GEOLOCATION_OVERRIDER_H_

#include "base/bind.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace device {

// A helper class which owns a FakeGeolocationContext by which the geolocation
// is overriden to a given position or latitude and longitude values.
// The FakeGeolocationContext overrides the binder of Device Service by
// service_manager::ServiceContext::SetGlobalBinderForTesting().
// The override of the geolocation implementation will be in effect for the
// duration of this object's lifetime.
class ScopedGeolocationOverrider {
 public:
  explicit ScopedGeolocationOverrider(const mojom::Geoposition& position);
  ScopedGeolocationOverrider(double latitude, double longitude);
  ~ScopedGeolocationOverrider();
  void OverrideGeolocation(const mojom::Geoposition& position);
  void UpdateLocation(const mojom::Geoposition& position);
  void UpdateLocation(double latitude, double longitude);

  // Pause resolving Geolocation queries to keep request inflight.
  // After |Pause()| call, Geolocation::QueryNextPosition does not respond,
  // allowing us to test behavior in the middle of the request.
  void Pause();

  // Resume resolving Geolocation queries.
  // Send the paused Geolocation::QueryNextPosition response.
  void Resume();

  // Count number of active FakeGeolocation instances, which is equal to the
  // number of active consumer Remote<Geolocation>s.
  // This is used to verify if consumers properly close the connections when
  // they should no longer be listening.
  size_t GetGeolocationInstanceCount() const;

  // Register callback to be notified when a Remote<Geolocation> is cleared and
  // the corresponding fake Geolocation instance is disposed.
  void SetGeolocationCloseCallback(base::RepeatingClosure closure);

 private:
  class FakeGeolocation;
  class FakeGeolocationContext;
  std::unique_ptr<FakeGeolocationContext> geolocation_context_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_SCOPED_GEOLOCATION_OVERRIDER_H_
