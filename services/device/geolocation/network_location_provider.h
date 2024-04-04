// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_NETWORK_LOCATION_PROVIDER_H_
#define SERVICES_DEVICE_GEOLOCATION_NETWORK_LOCATION_PROVIDER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "services/device/geolocation/network_location_request.h"
#include "services/device/geolocation/wifi_data_provider_handle.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/device/public/mojom/geolocation_internals.mojom.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace device {
class PositionCache;

class NetworkLocationProvider : public LocationProvider {
 public:
  using NetworkRequestCallback =
      base::RepeatingCallback<void(std::vector<mojom::AccessPointDataPtr>)>;
  using NetworkResponseCallback =
      base::RepeatingCallback<void(mojom::NetworkLocationResponsePtr)>;

  NetworkLocationProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& api_key,
      PositionCache* position_cache,
      base::RepeatingClosure internals_updated_closure,
      NetworkRequestCallback network_request_callback,
      NetworkResponseCallback network_response_callback);

  NetworkLocationProvider(const NetworkLocationProvider&) = delete;
  NetworkLocationProvider& operator=(const NetworkLocationProvider&) = delete;

  ~NetworkLocationProvider() override;

  // LocationProvider implementation
  void FillDiagnostics(mojom::GeolocationDiagnostics& diagnostics) override;
  void SetUpdateCallback(const LocationProviderUpdateCallback& cb) override;
  void StartProvider(bool high_accuracy) override;
  void StopProvider() override;
  const mojom::GeopositionResult* GetPosition() override;
  void OnPermissionGranted() override;

 private:
  // Tries to update |position_| request from cache or network.
  void RequestPosition();

  // Gets called when new wifi data is available, either via explicit request to
  // or callback from |wifi_data_provider_handle_|.
  void OnWifiDataUpdate();

  void OnLocationResponse(mojom::GeopositionResultPtr result,
                          bool server_error,
                          const WifiData& wifi_data,
                          mojom::NetworkLocationResponsePtr response_data);

  // The wifi data provider, acquired via global factories. Valid between
  // StartProvider() and StopProvider(), and checked via IsStarted().
  std::unique_ptr<WifiDataProviderHandle> wifi_data_provider_handle_;

  // True if the provider was started with high accuracy enabled.
  // `high_accuracy_` does not modify the behavior of this provider, it is only
  // stored for diagnostics.
  bool high_accuracy_ = false;

  WifiDataProviderHandle::WifiDataUpdateCallback wifi_data_update_callback_;

  // The  wifi data and a flag to indicate if the data set is complete.
  WifiData wifi_data_;
  bool is_wifi_data_complete_;

  // The timestamp for the latest wifi data update.
  base::Time wifi_timestamp_;

  const raw_ptr<PositionCache> position_cache_;

  LocationProvider::LocationProviderUpdateCallback
      location_provider_update_callback_;

  // Whether permission has been granted for the provider to operate.
  bool is_permission_granted_;

  bool is_new_data_available_;

  // The network location request object.
  const std::unique_ptr<NetworkLocationRequest> request_;

  base::ThreadChecker thread_checker_;

  base::RepeatingClosure internals_updated_closure_;

  // Called when a network request is sent to provide the request data to
  // diagnostics observers.
  NetworkRequestCallback network_request_callback_;

  // Called when a network response is received to provide the response data to
  // diagnostics observers.
  NetworkResponseCallback network_response_callback_;

  bool is_started_ = false;

  base::WeakPtrFactory<NetworkLocationProvider> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_NETWORK_LOCATION_PROVIDER_H_
