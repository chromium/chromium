// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_WIN_FAKE_GEOPOSITION_WINRT_H_
#define SERVICES_DEVICE_GEOLOCATION_WIN_FAKE_GEOPOSITION_WINRT_H_

#include <windows.devices.geolocation.h>
#include <windows.foundation.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <memory>

#include "base/macros.h"

namespace device {

struct FakeGeocoordinateData;

class FakeGeoposition
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Geolocation::IGeoposition> {
 public:
  explicit FakeGeoposition(
      std::unique_ptr<FakeGeocoordinateData> position_data);
  ~FakeGeoposition() override;
  IFACEMETHODIMP get_Coordinate(
      ABI::Windows::Devices::Geolocation::IGeocoordinate** value) override;
  IFACEMETHODIMP get_CivicAddress(
      ABI::Windows::Devices::Geolocation::ICivicAddress** value) override;

 private:
  std::unique_ptr<FakeGeocoordinateData> position_data_;
  DISALLOW_COPY_AND_ASSIGN(FakeGeoposition);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_WIN_FAKE_GEOPOSITION_WINRT_H_