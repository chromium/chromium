// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_WIN_FAKE_POSITION_CHANGED_EVENT_ARGS_WINRT_H_
#define SERVICES_DEVICE_GEOLOCATION_WIN_FAKE_POSITION_CHANGED_EVENT_ARGS_WINRT_H_

#include <windows.devices.geolocation.h>
#include <windows.foundation.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <memory>

#include "base/macros.h"

namespace device {

struct FakeGeocoordinateData;

class FakePositionChangedEventArgs
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Geolocation::IPositionChangedEventArgs> {
 public:
  explicit FakePositionChangedEventArgs(
      std::unique_ptr<FakeGeocoordinateData> position_data);
  ~FakePositionChangedEventArgs() override;
  IFACEMETHODIMP get_Position(
      ABI::Windows::Devices::Geolocation::IGeoposition** value) override;

 private:
  std::unique_ptr<FakeGeocoordinateData> position_data_;

  DISALLOW_COPY_AND_ASSIGN(FakePositionChangedEventArgs);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_WIN_FAKE_POSITION_CHANGED_EVENT_ARGS_WINRT_H_