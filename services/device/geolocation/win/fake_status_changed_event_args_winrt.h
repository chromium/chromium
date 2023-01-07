// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_WIN_FAKE_STATUS_CHANGED_EVENT_ARGS_WINRT_H_
#define SERVICES_DEVICE_GEOLOCATION_WIN_FAKE_STATUS_CHANGED_EVENT_ARGS_WINRT_H_

#include <windows.devices.geolocation.h>
#include <windows.foundation.h>
#include <wrl/client.h>
#include <wrl/implements.h>

namespace device {

class FakeStatusChangedEventArgs
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Geolocation::IStatusChangedEventArgs> {
 public:
  explicit FakeStatusChangedEventArgs(
      ABI::Windows::Devices::Geolocation::PositionStatus position_status);

  FakeStatusChangedEventArgs(const FakeStatusChangedEventArgs&) = delete;
  FakeStatusChangedEventArgs& operator=(const FakeStatusChangedEventArgs&) =
      delete;

  ~FakeStatusChangedEventArgs() override;
  IFACEMETHODIMP get_Status(
      ABI::Windows::Devices::Geolocation::PositionStatus* value) override;

 private:
  const ABI::Windows::Devices::Geolocation::PositionStatus position_status_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_WIN_FAKE_STATUS_CHANGED_EVENT_ARGS_WINRT_H_