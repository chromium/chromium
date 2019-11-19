// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_WIN_FAKE_GEOLOCATOR_WINRT_H_
#define SERVICES_DEVICE_GEOLOCATION_WIN_FAKE_GEOLOCATOR_WINRT_H_

#include <windows.devices.geolocation.h>
#include <windows.foundation.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"

namespace device {

struct FakeGeocoordinateData;

class FakeGeolocatorWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Geolocation::IGeolocator> {
 public:
  FakeGeolocatorWinrt(
      std::unique_ptr<FakeGeocoordinateData> position_data,
      ABI::Windows::Devices::Geolocation::PositionStatus position_status);
  ~FakeGeolocatorWinrt() override;

  // IGeolocator:
  IFACEMETHODIMP get_DesiredAccuracy(
      ABI::Windows::Devices::Geolocation::PositionAccuracy* value) override;
  IFACEMETHODIMP put_DesiredAccuracy(
      ABI::Windows::Devices::Geolocation::PositionAccuracy value) override;
  IFACEMETHODIMP get_MovementThreshold(DOUBLE* value) override;
  IFACEMETHODIMP put_MovementThreshold(DOUBLE value) override;
  IFACEMETHODIMP get_ReportInterval(UINT32* value) override;
  IFACEMETHODIMP put_ReportInterval(UINT32 value) override;
  IFACEMETHODIMP get_LocationStatus(
      ABI::Windows::Devices::Geolocation::PositionStatus* value) override;
  IFACEMETHODIMP GetGeopositionAsync(
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Geolocation::Geoposition*>** value) override;
  IFACEMETHODIMP GetGeopositionAsyncWithAgeAndTimeout(
      ABI::Windows::Foundation::TimeSpan maximumAge,
      ABI::Windows::Foundation::TimeSpan timeout,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Geolocation::Geoposition*>** value) override;
  IFACEMETHODIMP add_PositionChanged(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Geolocation::Geolocator*,
          ABI::Windows::Devices::Geolocation::PositionChangedEventArgs*>*
          handler,
      EventRegistrationToken* token) override;
  IFACEMETHODIMP remove_PositionChanged(EventRegistrationToken token) override;
  IFACEMETHODIMP add_StatusChanged(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Geolocation::Geolocator*,
          ABI::Windows::Devices::Geolocation::StatusChangedEventArgs*>* handler,
      EventRegistrationToken* token) override;
  IFACEMETHODIMP remove_StatusChanged(EventRegistrationToken token) override;

 private:
  void RunPositionChangedHandler(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Geolocation::Geolocator*,
          ABI::Windows::Devices::Geolocation::PositionChangedEventArgs*>*
          handler);
  void RunStatusChangedHandler(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Geolocation::Geolocator*,
          ABI::Windows::Devices::Geolocation::StatusChangedEventArgs*>*
          handler);

  ABI::Windows::Devices::Geolocation::PositionAccuracy accuracy_;
  DOUBLE movement_threshold_ = 0;
  base::Optional<EventRegistrationToken> position_changed_token_;
  base::Optional<EventRegistrationToken> status_changed_token_;
  std::unique_ptr<FakeGeocoordinateData> position_data_;
  const ABI::Windows::Devices::Geolocation::PositionStatus position_status_;

  base::WeakPtrFactory<FakeGeolocatorWinrt> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeGeolocatorWinrt);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_WIN_FAKE_GEOLOCATOR_WINRT_H_