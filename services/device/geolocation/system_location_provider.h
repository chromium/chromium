// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_SYSTEM_LOCATION_PROVIDER_H_
#define SERVICES_DEVICE_GEOLOCATION_SYSTEM_LOCATION_PROVIDER_H_

#include "services/device/public/cpp/geolocation/location_provider.h"

namespace device {

class SystemLocationProvider : public LocationProvider {
 public:
  using ShouldUseCallback = base::RepeatingCallback<void(bool)>;

  // SystemLocationProviders are sometimes flaky (so far this is only used for
  // macOS). Therefore the LocationArbitrator will default to using the
  // NetworkLocationProvider until the SystemLocationProvider determines it is
  // working properly. At this time the provided callback should be called with
  // |should_use|=true. If at any time the SystemLocationProvider stops working
  // this callback should again be used with |should_use|=false to notify the
  // LocationArbitrator that it can no longer rely on the
  // SystemLocationProvider. For example the macOS SystemLocationProvider no
  // longer works when the WiFi adapter is turned off.
  virtual void SetShouldUseSystemProviderCallback(
      const ShouldUseCallback& callback) {}
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_SYSTEM_LOCATION_PROVIDER_H_