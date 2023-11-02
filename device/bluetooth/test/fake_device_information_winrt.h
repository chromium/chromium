// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_INFORMATION_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_INFORMATION_WINRT_H_

#include <windows.devices.enumeration.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <string>

#include "device/bluetooth/test/fake_device_information_pairing_winrt.h"

namespace device {

class FakeDeviceInformationWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Enumeration::IDeviceInformation,
          ABI::Windows::Devices::Enumeration::IDeviceInformation2> {
 public:
  FakeDeviceInformationWinrt();
  // Explicit const char* constructor is required to break ambiguity for C
  // string arguments.
  explicit FakeDeviceInformationWinrt(const char* name);
  explicit FakeDeviceInformationWinrt(std::string name);
  explicit FakeDeviceInformationWinrt(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Devices::Enumeration::IDeviceInformationPairing>
          pairing);

  FakeDeviceInformationWinrt(const FakeDeviceInformationWinrt&) = delete;
  FakeDeviceInformationWinrt& operator=(const FakeDeviceInformationWinrt&) =
      delete;

  ~FakeDeviceInformationWinrt() override;

  // IDeviceInformation:
  IFACEMETHODIMP get_Id(HSTRING* value) override;
  IFACEMETHODIMP get_Name(HSTRING* value) override;
  IFACEMETHODIMP get_IsEnabled(boolean* value) override;
  IFACEMETHODIMP get_IsDefault(boolean* value) override;
  IFACEMETHODIMP get_EnclosureLocation(
      ABI::Windows::Devices::Enumeration::IEnclosureLocation** value) override;
  IFACEMETHODIMP get_Properties(
      ABI::Windows::Foundation::Collections::IMapView<HSTRING, IInspectable*>**
          value) override;
  IFACEMETHODIMP Update(
      ABI::Windows::Devices::Enumeration::IDeviceInformationUpdate* update_info)
      override;
  IFACEMETHODIMP GetThumbnailAsync(
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Enumeration::DeviceThumbnail*>** async_op)
      override;
  IFACEMETHODIMP GetGlyphThumbnailAsync(
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Enumeration::DeviceThumbnail*>** async_op)
      override;

  // IDeviceInformation2:
  IFACEMETHODIMP get_Kind(
      ABI::Windows::Devices::Enumeration::DeviceInformationKind* value)
      override;
  IFACEMETHODIMP get_Pairing(
      ABI::Windows::Devices::Enumeration::IDeviceInformationPairing** value)
      override;

 private:
  std::string name_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::Devices::Enumeration::IDeviceInformationPairing>
      pairing_ = Microsoft::WRL::Make<FakeDeviceInformationPairingWinrt>(false);
};

class FakeDeviceInformationStaticsWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Enumeration::IDeviceInformationStatics> {
 public:
  explicit FakeDeviceInformationStaticsWinrt(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Devices::Enumeration::IDeviceInformation>
          device_information);

  FakeDeviceInformationStaticsWinrt(const FakeDeviceInformationStaticsWinrt&) =
      delete;
  FakeDeviceInformationStaticsWinrt& operator=(
      const FakeDeviceInformationStaticsWinrt&) = delete;

  ~FakeDeviceInformationStaticsWinrt() override;

  // IDeviceInformationStatics:
  IFACEMETHODIMP CreateFromIdAsync(
      HSTRING device_id,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Enumeration::DeviceInformation*>** async_op)
      override;
  IFACEMETHODIMP CreateFromIdAsyncAdditionalProperties(
      HSTRING device_id,
      ABI::Windows::Foundation::Collections::IIterable<HSTRING>*
          additional_properties,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Enumeration::DeviceInformation*>** async_op)
      override;
  IFACEMETHODIMP FindAllAsync(
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Enumeration::DeviceInformationCollection*>**
          async_op) override;
  IFACEMETHODIMP FindAllAsyncDeviceClass(
      ABI::Windows::Devices::Enumeration::DeviceClass device_class,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Enumeration::DeviceInformationCollection*>**
          async_op) override;
  IFACEMETHODIMP FindAllAsyncAqsFilter(
      HSTRING aqs_filter,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Enumeration::DeviceInformationCollection*>**
          async_op) override;
  IFACEMETHODIMP
  FindAllAsyncAqsFilterAndAdditionalProperties(
      HSTRING aqs_filter,
      ABI::Windows::Foundation::Collections::IIterable<HSTRING>*
          additional_properties,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Enumeration::DeviceInformationCollection*>**
          async_op) override;
  IFACEMETHODIMP CreateWatcher(
      ABI::Windows::Devices::Enumeration::IDeviceWatcher** watcher) override;
  IFACEMETHODIMP CreateWatcherDeviceClass(
      ABI::Windows::Devices::Enumeration::DeviceClass device_class,
      ABI::Windows::Devices::Enumeration::IDeviceWatcher** watcher) override;
  IFACEMETHODIMP CreateWatcherAqsFilter(
      HSTRING aqs_filter,
      ABI::Windows::Devices::Enumeration::IDeviceWatcher** watcher) override;
  IFACEMETHODIMP
  CreateWatcherAqsFilterAndAdditionalProperties(
      HSTRING aqs_filter,
      ABI::Windows::Foundation::Collections::IIterable<HSTRING>*
          additional_properties,
      ABI::Windows::Devices::Enumeration::IDeviceWatcher** watcher) override;

 private:
  Microsoft::WRL::ComPtr<ABI::Windows::Devices::Enumeration::IDeviceInformation>
      device_information_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_INFORMATION_WINRT_H_
