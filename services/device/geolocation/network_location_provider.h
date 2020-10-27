// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_NETWORK_LOCATION_PROVIDER_H_
#define SERVICES_DEVICE_GEOLOCATION_NETWORK_LOCATION_PROVIDER_H_

#include <stddef.h>

#include <list>
#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "services/device/geolocation/network_location_request.h"
#include "services/device/geolocation/wifi_data_provider_manager.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace device {
class PositionCache;
class NetworkLocationProvider : public LocationProvider {
 public:
  NetworkLocationProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& api_key,
      PositionCache* position_cache);
  ~NetworkLocationProvider() override;

  // LocationProvider implementation
  void SetUpdateCallback(const LocationProviderUpdateCallback& cb) override;
  void StartProvider(bool high_accuracy) override;
  void StopProvider() override;
  const mojom::Geoposition& GetPosition() override;
  void OnPermissionGranted() override;

 private:
  // Tries to update |position_| request from cache or network.
  void RequestPosition();

  // Gets called when new wifi data is available, either via explicit request to
  // or callback from |wifi_data_provider_manager_|.
  void OnWifiDataUpdate();

  bool IsStarted() const;

  void OnLocationResponse(const mojom::Geoposition& position,
                          bool server_error,
                          const WifiData& wifi_data);

  // The wifi data provider, acquired via global factories. Valid between
  // StartProvider() and StopProvider(), and checked via IsStarted().
  WifiDataProviderManager* wifi_data_provider_manager_;

  WifiDataProviderManager::WifiDataUpdateCallback wifi_data_update_callback_;

  // The  wifi data and a flag to indicate if the data set is complete.
  WifiData wifi_data_;
  bool is_wifi_data_complete_;

  // The timestamp for the latest wifi data update.
  base::Time wifi_timestamp_;

  PositionCache* const position_cache_;

  LocationProvider::LocationProviderUpdateCallback
      location_provider_update_callback_;

  // Whether permission has been granted for the provider to operate.
  bool is_permission_granted_;

  bool is_new_data_available_;

  // The network location request object.
  const std::unique_ptr<NetworkLocationRequest> request_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<NetworkLocationProvider> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NetworkLocationProvider);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_NETWORK_LOCATION_PROVIDER_H_
