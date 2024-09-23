// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_LOCATION_PROVIDER_MANAGER_H_
#define SERVICES_DEVICE_GEOLOCATION_LOCATION_PROVIDER_MANAGER_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "services/device/geolocation/geolocation_provider_impl.h"
#include "services/device/geolocation/network_location_provider.h"
#include "services/device/geolocation/position_cache.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/device/public/mojom/geolocation_internals.mojom.h"
#include "services/device/public/mojom/geoposition.mojom.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace device {

class GeolocationSystemPermissionManager;

// `LocationProviderManager` manages the selection and lifecycle of location
// providers based on platform capabilities and feature parameters. Supports
// mode-based selection (platform, network, custom, hybrid) and fallback
// mechanism for error position handling.
class LocationProviderManager : public LocationProvider {
 public:
  LocationProviderManager(
      CustomLocationProviderCallback custom_location_provider_getter,
      GeolocationSystemPermissionManager* geolocation_system_permission_manager,
      const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
      const std::string& api_key,
      std::unique_ptr<PositionCache> position_cache,
      base::RepeatingClosure internals_updated_closure,
      NetworkLocationProvider::NetworkRequestCallback network_request_callback,
      NetworkLocationProvider::NetworkResponseCallback
          network_response_callback);
  LocationProviderManager(const LocationProviderManager&) = delete;
  LocationProviderManager& operator=(const LocationProviderManager&) = delete;
  ~LocationProviderManager() override;

  static GURL DefaultNetworkProviderURL();
  bool HasPermissionBeenGrantedForTest() const;

  // LocationProvider implementation.
  void FillDiagnostics(mojom::GeolocationDiagnostics& diagnostics) override;
  void SetUpdateCallback(
      const LocationProviderUpdateCallback& callback) override;
  void StartProvider(bool enable_high_accuracy) override;
  void StopProvider() override;
  const mojom::GeopositionResult* GetPosition() override;
  void OnPermissionGranted() override;

 protected:
  // These functions are useful for injection of dependencies in derived
  // testing classes.
  virtual std::unique_ptr<LocationProvider> NewNetworkLocationProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& api_key);
  virtual std::unique_ptr<LocationProvider> NewSystemLocationProvider();

 private:
  friend class TestingLocationProviderManager;
  friend class GeolocationLocationProviderManagerTest;
  FRIEND_TEST_ALL_PREFIXES(GeolocationLocationProviderManagerTest,
                           HybridPlatformFallback);
  void RegisterProvider(LocationProvider& provider);

  // Initializes the appropriate LocationProvider based on the configured mode
  // (network, platform, custom, or hybrid). Returns true if a provider was
  // successfully created and registered (or already done previously), false
  // otherwise.
  bool InitializeProvider();

  // Gets called when a provider has a new position.
  void OnLocationUpdate(const LocationProvider* provider,
                        mojom::GeopositionResultPtr new_result);


  const CustomLocationProviderCallback custom_location_provider_getter_;
  const raw_ptr<GeolocationSystemPermissionManager, DanglingUntriaged>
      geolocation_system_permission_manager_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const std::string api_key_;

  bool enable_high_accuracy_;
  bool is_permission_granted_ = false;
  bool is_running_ = false;  // Tracks whether providers should be running.
  LocationProvider::LocationProviderUpdateCallback location_update_callback_;
  std::unique_ptr<PositionCache> position_cache_;  // must outlive `providers_`

  // LocationProviders.
  std::unique_ptr<LocationProvider> network_location_provider_;
  std::unique_ptr<LocationProvider> platform_location_provider_;
  std::unique_ptr<LocationProvider> custom_location_provider_;

  // Result cache from each provider.
  mojom::GeopositionResultPtr network_location_provider_result_;
  mojom::GeopositionResultPtr platform_location_provider_result_;
  mojom::GeopositionResultPtr custom_location_provider_result_;

  // To be called when a provider's internal diagnostics have changed.
  base::RepeatingClosure internals_updated_closure_;
  // Callbacks to be called by NetworkLocationProvider when network requests are
  // sent and received.
  NetworkLocationProvider::NetworkRequestCallback network_request_callback_;
  NetworkLocationProvider::NetworkResponseCallback network_response_callback_;

  // Indicates the operating mode for LocationProviderManager and which
  // provider is currently active (custom, network, or platform). Also
  // determines if the fallback mechanism is used (kHybridPlatform or
  // kHybridFallbackNetwork).
  mojom::LocationProviderManagerMode provider_manager_mode_;
};

// Factory functions for the various types of location provider to abstract
// over the platform-dependent implementations.
#if BUILDFLAG(IS_APPLE)
std::unique_ptr<LocationProvider> NewSystemLocationProvider(
    SystemGeolocationSource& system_geolocation_source);
#else
std::unique_ptr<LocationProvider> NewSystemLocationProvider();
#endif

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_LOCATION_PROVIDER_MANAGER_H_
