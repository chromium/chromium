// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_WIN_LOCATION_PROVIDER_WINRT_H_
#define SERVICES_DEVICE_GEOLOCATION_WIN_LOCATION_PROVIDER_WINRT_H_

#include <windows.devices.geolocation.h>
#include <wrl/client.h>

#include "base/threading/thread_checker.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace device {

// Location provider for Windows 8/10 using the WinRT platform apis
class LocationProviderWinrt : public LocationProvider {
 public:
  LocationProviderWinrt();
  ~LocationProviderWinrt() override;

  // LocationProvider implementation.
  void SetUpdateCallback(
      const LocationProviderUpdateCallback& callback) override;
  void StartProvider(bool high_accuracy) override;
  void StopProvider() override;
  const mojom::Geoposition& GetPosition() override;
  void OnPermissionGranted() override;

 protected:
  virtual HRESULT GetGeolocator(
      ABI::Windows::Devices::Geolocation::IGeolocator** geo_locator);

  bool permission_granted_ = false;
  bool enable_high_accuracy_ = false;
  base::Optional<EventRegistrationToken> position_changed_token_;
  base::Optional<EventRegistrationToken> status_changed_token_;

 private:
  void HandleErrorCondition(mojom::Geoposition::ErrorCode position_error_code,
                            const std::string& position_error_message);
  void RegisterCallbacks();
  void UnregisterCallbacks();
  void OnPositionChanged(
      ABI::Windows::Devices::Geolocation::IGeolocator* geo_locator,
      ABI::Windows::Devices::Geolocation::IPositionChangedEventArgs*
          position_update);
  void OnStatusChanged(
      ABI::Windows::Devices::Geolocation::IGeolocator* geo_locator,
      ABI::Windows::Devices::Geolocation::IStatusChangedEventArgs*
          status_update);
  void PopulateLocationData(
      ABI::Windows::Devices::Geolocation::IGeoposition* geoposition,
      mojom::Geoposition* location_data);

  mojom::Geoposition last_position_;
  LocationProviderUpdateCallback location_update_callback_;
  Microsoft::WRL::ComPtr<ABI::Windows::Devices::Geolocation::IGeolocator>
      geo_locator_;
  bool position_received_ = false;
  ABI::Windows::Devices::Geolocation::PositionStatus position_status_;
  base::TimeTicks position_callback_initialized_time_;
  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<LocationProviderWinrt> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LocationProviderWinrt);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_WIN_LOCATION_PROVIDER_WINRT_H_
