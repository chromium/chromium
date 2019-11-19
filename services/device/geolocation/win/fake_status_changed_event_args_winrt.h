// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_WIN_FAKE_STATUS_CHANGED_EVENT_ARGS_WINRT_H_
#define SERVICES_DEVICE_GEOLOCATION_WIN_FAKE_STATUS_CHANGED_EVENT_ARGS_WINRT_H_

#include <windows.devices.geolocation.h>
#include <windows.foundation.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/macros.h"

namespace device {

class FakeStatusChangedEventArgs
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Geolocation::IStatusChangedEventArgs> {
 public:
  explicit FakeStatusChangedEventArgs(
      ABI::Windows::Devices::Geolocation::PositionStatus position_status);
  ~FakeStatusChangedEventArgs() override;
  IFACEMETHODIMP get_Status(
      ABI::Windows::Devices::Geolocation::PositionStatus* value) override;

 private:
  const ABI::Windows::Devices::Geolocation::PositionStatus position_status_;

  DISALLOW_COPY_AND_ASSIGN(FakeStatusChangedEventArgs);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_WIN_FAKE_STATUS_CHANGED_EVENT_ARGS_WINRT_H_