// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_WIN_LOCATION_PROVIDER_WINRT_H_
#define SERVICES_DEVICE_GEOLOCATION_WIN_LOCATION_PROVIDER_WINRT_H_

#include <windows.devices.geolocation.h>
#include <wrl/client.h>

#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/device/public/mojom/geolocation_internals.mojom.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace device {

// Location provider for Windows 8/10 using the WinRT platform apis
class LocationProviderWinrt : public LocationProvider {
 public:
  LocationProviderWinrt();

  LocationProviderWinrt(const LocationProviderWinrt&) = delete;
  LocationProviderWinrt& operator=(const LocationProviderWinrt&) = delete;

  ~LocationProviderWinrt() override;

  // LocationProvider implementation.
  void FillDiagnostics(mojom::GeolocationDiagnostics& diagnostics) override;
  void SetUpdateCallback(
      const LocationProviderUpdateCallback& callback) override;
  void StartProvider(bool high_accuracy) override;
  void StopProvider() override;
  const mojom::GeopositionResult* GetPosition() override;
  void OnPermissionGranted() override;

 protected:
  virtual HRESULT GetGeolocator(
      ABI::Windows::Devices::Geolocation::IGeolocator** geo_locator);

  bool permission_granted_ = false;
  bool enable_high_accuracy_ = false;
  std::optional<EventRegistrationToken> position_changed_token_;
  std::optional<EventRegistrationToken> status_changed_token_;

 private:
  // Returns true if `last_result_` contains a valid geoposition.
  bool HasValidLastPosition() const;

  void HandleErrorCondition(mojom::GeopositionErrorCode position_error_code,
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
  mojom::GeopositionPtr CreateGeoposition(
      ABI::Windows::Devices::Geolocation::IGeoposition* geoposition);
  void SetSessionErrorIfNotSet(HRESULT error);

  bool is_started_ = false;
  mojom::GeopositionResultPtr last_result_;
  LocationProviderUpdateCallback location_update_callback_;
  Microsoft::WRL::ComPtr<ABI::Windows::Devices::Geolocation::IGeolocator>
      geo_locator_;
  bool position_received_ = false;
  ABI::Windows::Devices::Geolocation::PositionStatus position_status_;
  base::TimeTicks position_callback_initialized_time_;

  // Records any error code encountered that stops the location provider from
  // getting its a valid Geoposition.
  std::optional<HRESULT> session_error_;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<LocationProviderWinrt> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_WIN_LOCATION_PROVIDER_WINRT_H_
