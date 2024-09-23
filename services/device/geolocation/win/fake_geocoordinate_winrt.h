// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_WIN_FAKE_GEOCOORDINATE_WINRT_H_
#define SERVICES_DEVICE_GEOLOCATION_WIN_FAKE_GEOCOORDINATE_WINRT_H_

#include <windows.devices.geolocation.h>
#include <windows.foundation.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <memory>
#include <optional>

namespace device {

struct FakeGeocoordinateData {
  FakeGeocoordinateData();
  FakeGeocoordinateData(const FakeGeocoordinateData& obj);
  DOUBLE latitude = 0;
  DOUBLE longitude = 0;
  DOUBLE accuracy = 0;
  std::optional<DOUBLE> altitude;
  std::optional<DOUBLE> altitude_accuracy;
  std::optional<DOUBLE> heading;
  std::optional<DOUBLE> speed;
  std::optional<ABI::Windows::Devices::Geolocation::AltitudeReferenceSystem>
      altitude_reference_system;
};

class FakeGeopoint
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Geolocation::IGeopoint,
          ABI::Windows::Devices::Geolocation::IGeoshape> {
 public:
  explicit FakeGeopoint(const FakeGeocoordinateData& position_data);
  FakeGeopoint(const FakeGeopoint&) = delete;
  FakeGeopoint(FakeGeopoint&&) = delete;
  FakeGeopoint& operator=(const FakeGeopoint&) = delete;
  FakeGeopoint& operator=(FakeGeopoint&&) = delete;
  ~FakeGeopoint() override;

  // IGeopoint implementation:
  IFACEMETHODIMP get_Position(
      ABI::Windows::Devices::Geolocation::BasicGeoposition* value) override;

  // IGeoshape implementation:
  IFACEMETHODIMP get_GeoshapeType(
      ABI::Windows::Devices::Geolocation::GeoshapeType* value) override;
  IFACEMETHODIMP get_SpatialReferenceId(UINT32* value) override;
  IFACEMETHODIMP get_AltitudeReferenceSystem(
      ABI::Windows::Devices::Geolocation::AltitudeReferenceSystem* value)
      override;

 private:
  const FakeGeocoordinateData position_data_;
};

class FakeGeocoordinate
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Geolocation::IGeocoordinate,
          ABI::Windows::Devices::Geolocation::IGeocoordinateWithPoint> {
 public:
  explicit FakeGeocoordinate(
      std::unique_ptr<FakeGeocoordinateData> position_data);

  FakeGeocoordinate(const FakeGeocoordinate&) = delete;
  FakeGeocoordinate& operator=(const FakeGeocoordinate&) = delete;

  ~FakeGeocoordinate() override;
  IFACEMETHODIMP get_Point(
      ABI::Windows::Devices::Geolocation::IGeopoint** point) override;
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
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_WIN_FAKE_GEOCOORDINATE_WINRT_H_
