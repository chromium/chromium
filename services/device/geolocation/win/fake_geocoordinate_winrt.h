// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_WIN_FAKE_GEOCOORDINATE_WINRT_H_
#define SERVICES_DEVICE_GEOLOCATION_WIN_FAKE_GEOCOORDINATE_WINRT_H_

#include <windows.devices.geolocation.h>
#include <windows.foundation.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/macros.h"
#include "base/optional.h"

namespace device {

struct FakeGeocoordinateData {
  FakeGeocoordinateData();
  FakeGeocoordinateData(const FakeGeocoordinateData& obj);
  DOUBLE latitude = 0;
  DOUBLE longitude = 0;
  DOUBLE accuracy = 0;
  base::Optional<DOUBLE> altitude;
  base::Optional<DOUBLE> altitude_accuracy;
  base::Optional<DOUBLE> heading;
  base::Optional<DOUBLE> speed;
};

class FakeGeocoordinate
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Geolocation::IGeocoordinate> {
 public:
  explicit FakeGeocoordinate(
      std::unique_ptr<FakeGeocoordinateData> position_data);
  ~FakeGeocoordinate() override;
  IFACEMETHODIMP get_Latitude(DOUBLE* value) override;
  IFACEMETHODIMP get_Longitude(DOUBLE* value) override;
  IFACEMETHODIMP get_Altitude(
      ABI::Windows::Foundation::IReference<double>** value) override;
  IFACEMETHODIMP get_Accuracy(DOUBLE* value) override;
  IFACEMETHODIMP get_AltitudeAccuracy(
      ABI::Windows::Foundation::IReference<double>** value) override;
  IFACEMETHODIMP get_Heading(
      ABI::Windows::Foundation::IReference<double>** value) override;
  IFACEMETHODIMP get_Speed(
      ABI::Windows::Foundation::IReference<double>** value) override;
  IFACEMETHODIMP get_Timestamp(
      ABI::Windows::Foundation::DateTime* value) override;

 private:
  std::unique_ptr<FakeGeocoordinateData> position_data_;

  DISALLOW_COPY_AND_ASSIGN(FakeGeocoordinate);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_WIN_FAKE_GEOCOORDINATE_WINRT_H_