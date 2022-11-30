// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_WATCHER_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_WATCHER_WINRT_H_

#include <windows.devices.enumeration.h>
#include <windows.foundation.h>
#include <wrl/client.h>
#include <wrl/implements.h>

namespace device {

class FakeDeviceWatcherWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Enumeration::IDeviceWatcher> {
 public:
  FakeDeviceWatcherWinrt();

  FakeDeviceWatcherWinrt(const FakeDeviceWatcherWinrt&) = delete;
  FakeDeviceWatcherWinrt& operator=(const FakeDeviceWatcherWinrt&) = delete;

  ~FakeDeviceWatcherWinrt() override;

  // IDeviceWatcher:
  IFACEMETHODIMP add_Added(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Enumeration::DeviceWatcher*,
          ABI::Windows::Devices::Enumeration::DeviceInformation*>* handler,
      EventRegistrationToken* token) override;
  IFACEMETHODIMP remove_Added(EventRegistrationToken token) override;
  IFACEMETHODIMP add_Updated(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Enumeration::DeviceWatcher*,
          ABI::Windows::Devices::Enumeration::DeviceInformationUpdate*>*
          handler,
      EventRegistrationToken* token) override;
  IFACEMETHODIMP remove_Updated(EventRegistrationToken token) override;
  IFACEMETHODIMP add_Removed(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Enumeration::DeviceWatcher*,
          ABI::Windows::Devices::Enumeration::DeviceInformationUpdate*>*
          handler,
      EventRegistrationToken* token) override;
  IFACEMETHODIMP remove_Removed(EventRegistrationToken token) override;
  IFACEMETHODIMP add_EnumerationCompleted(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Enumeration::DeviceWatcher*,
          IInspectable*>* handler,
      EventRegistrationToken* token) override;
  IFACEMETHODIMP remove_EnumerationCompleted(
      EventRegistrationToken token) override;
  IFACEMETHODIMP add_Stopped(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Enumeration::DeviceWatcher*,
          IInspectable*>* handler,
      EventRegistrationToken* token) override;
  IFACEMETHODIMP remove_Stopped(EventRegistrationToken token) override;
  IFACEMETHODIMP get_Status(
      ABI::Windows::Devices::Enumeration::DeviceWatcherStatus* status) override;
  IFACEMETHODIMP Start() override;
  IFACEMETHODIMP Stop() override;

  void SimulateAdapterPoweredOn();
  void SimulateAdapterPoweredOff();

 private:
  bool has_powered_radio_ = false;

  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::ITypedEventHandler<
      ABI::Windows::Devices::Enumeration::DeviceWatcher*,
      ABI::Windows::Devices::Enumeration::DeviceInformation*>>
      added_handler_;

  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::ITypedEventHandler<
      ABI::Windows::Devices::Enumeration::DeviceWatcher*,
      ABI::Windows::Devices::Enumeration::DeviceInformationUpdate*>>
      removed_handler_;

  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::ITypedEventHandler<
      ABI::Windows::Devices::Enumeration::DeviceWatcher*,
      IInspectable*>>
      enumerated_handler_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_WATCHER_WINRT_H_
