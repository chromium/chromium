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
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "services/device/geolocation/network_location_request.h"
#include "services/device/geolocation/wifi_data_provider_handle.h"
#include "services/device/public/cpp/geolocation/geolocation_manager.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace device {
class PositionCache;

class NetworkLocationProvider : public LocationProvider
#if BUILDFLAG(IS_MAC)
    ,
                                public GeolocationManager::PermissionObserver
#endif
{
 public:
  NetworkLocationProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GeolocationManager* geolocation_manager,
      const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      const std::string& api_key,
      PositionCache* position_cache);

  NetworkLocationProvider(const NetworkLocationProvider&) = delete;
  NetworkLocationProvider& operator=(const NetworkLocationProvider&) = delete;

  ~NetworkLocationProvider() override;

  // LocationProvider implementation
  void SetUpdateCallback(const LocationProviderUpdateCallback& cb) override;
  void StartProvider(bool high_accuracy) override;
  void StopProvider() override;
  const mojom::GeopositionResult* GetPosition() override;
  void OnPermissionGranted() override;

#if BUILDFLAG(IS_MAC)
  // GeolocationPermissionObserver implementation.
  void OnSystemPermissionUpdated(
      LocationSystemPermissionStatus new_status) override;
#endif

 private:
  // Tries to update |position_| request from cache or network.
  void RequestPosition();

  // Gets called when new wifi data is available, either via explicit request to
  // or callback from |wifi_data_provider_handle_|.
  void OnWifiDataUpdate();

  bool IsStarted() const;

  void OnLocationResponse(mojom::GeopositionResultPtr result,
                          bool server_error,
                          const WifiData& wifi_data);

  // The wifi data provider, acquired via global factories. Valid between
  // StartProvider() and StopProvider(), and checked via IsStarted().
  std::unique_ptr<WifiDataProviderHandle> wifi_data_provider_handle_;

  WifiDataProviderHandle::WifiDataUpdateCallback wifi_data_update_callback_;

#if BUILDFLAG(IS_MAC)
  // Used to keep track of macOS System Permission changes. Also, ensures
  // lifetime of PermissionObserverList as the BrowserProcess may destroy its
  // reference on the UI Thread before we destroy this provider.
  scoped_refptr<GeolocationManager::PermissionObserverList>
      permission_observers_;

  raw_ptr<GeolocationManager> geolocation_manager_;
#endif

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

#if BUILDFLAG(IS_MAC)
  bool is_system_permission_granted_ = false;

  bool is_awaiting_initial_permission_status_ = true;
#endif

  base::WeakPtrFactory<NetworkLocationProvider> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_NETWORK_LOCATION_PROVIDER_H_
