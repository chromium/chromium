// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_LOCATION_ARBITRATOR_H_
#define SERVICES_DEVICE_GEOLOCATION_LOCATION_ARBITRATOR_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "services/device/geolocation/geolocation_provider_impl.h"
#include "services/device/geolocation/network_location_provider.h"
#include "services/device/geolocation/position_cache.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/device/public/mojom/geoposition.mojom.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace device {

// This class is responsible for handling updates from multiple underlying
// providers and resolving them to a single 'best' location fix at any given
// moment.
class LocationArbitrator : public LocationProvider {
 public:
  // The TimeDelta newer a location provider has to be that it's worth
  // switching to this location provider on the basis of it being fresher
  // (regardles of relative accuracy). Public for tests.
  static const base::TimeDelta kFixStaleTimeoutTimeDelta;

  // If the |custom_location_provider_getter| callback returns nullptr, then
  // LocationArbitrator uses the default system location provider.
  LocationArbitrator(
      const CustomLocationProviderCallback& custom_location_provider_getter,
      const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& api_key,
      std::unique_ptr<PositionCache> position_cache);
  ~LocationArbitrator() override;

  static GURL DefaultNetworkProviderURL();
  bool HasPermissionBeenGrantedForTest() const;

  // LocationProvider implementation.
  void SetUpdateCallback(
      const LocationProviderUpdateCallback& callback) override;
  void StartProvider(bool enable_high_accuracy) override;
  void StopProvider() override;
  const mojom::Geoposition& GetPosition() override;
  void OnPermissionGranted() override;

 protected:
  // These functions are useful for injection of dependencies in derived
  // testing classes.
  virtual std::unique_ptr<LocationProvider> NewNetworkLocationProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& api_key);
  virtual std::unique_ptr<LocationProvider> NewSystemLocationProvider();
  virtual base::Time GetTimeNow() const;

 private:
  friend class TestingLocationArbitrator;

  // Provider will either be added to |providers_| or
  // deleted on error (e.g. it fails to start).
  void RegisterProvider(std::unique_ptr<LocationProvider> provider);
  void RegisterProviders();

  // Tells all registered providers to start.
  // If |providers_| is empty, immediately provides
  // Geoposition::ERROR_CODE_POSITION_UNAVAILABLE to the client via
  // |arbitrator_update_callback_|.
  void DoStartProviders();

  // Gets called when a provider has a new position.
  void OnLocationUpdate(const LocationProvider* provider,
                        const mojom::Geoposition& new_position);

  // Returns true if |new_position| is an improvement over |old_position|.
  // Set |from_same_provider| to true if both the positions came from the same
  // provider.
  bool IsNewPositionBetter(const mojom::Geoposition& old_position,
                           const mojom::Geoposition& new_position,
                           bool from_same_provider) const;

  const CustomLocationProviderCallback custom_location_provider_getter_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const std::string api_key_;

  LocationProvider::LocationProviderUpdateCallback arbitrator_update_callback_;

  std::vector<std::unique_ptr<LocationProvider>> providers_;
  bool enable_high_accuracy_;
  // The provider which supplied the current |position_|
  const LocationProvider* position_provider_;
  bool is_permission_granted_;
  // The current best estimate of our position.
  mojom::Geoposition position_;

  std::unique_ptr<PositionCache> position_cache_;

  // Tracks whether providers should be running.
  bool is_running_;

  DISALLOW_COPY_AND_ASSIGN(LocationArbitrator);
};

// Factory functions for the various types of location provider to abstract
// over the platform-dependent implementations.
std::unique_ptr<LocationProvider> NewSystemLocationProvider();

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_LOCATION_ARBITRATOR_H_
