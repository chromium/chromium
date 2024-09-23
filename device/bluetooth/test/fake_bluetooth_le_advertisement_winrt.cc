// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_bluetooth_le_advertisement_winrt.h"

#include <windows.foundation.collections.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/win/reference.h"
#include "base/win/scoped_hstring.h"
#include "base/win/vector.h"
#include "device/bluetooth/bluetooth_device_winrt.h"
#include "device/bluetooth/test/fake_bluetooth_le_advertisement_data_section_winrt.h"
#include "device/bluetooth/test/fake_bluetooth_le_manufacturer_data_winrt.h"

namespace {

using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementDataSection;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementFlags;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEManufacturerData;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisementDataSection;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEManufacturerData;
using ABI::Windows::Foundation::IReference;
using ABI::Windows::Foundation::Collections::IVector;
using ABI::Windows::Foundation::Collections::IVectorView;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

}  // namespace

// Note: As UWP does not provide GUID and  specializations for all required
// templates we need to supply our own. UUIDs were generated using `uuidgen`.
namespace ABI {
namespace Windows {
namespace Foundation {
namespace Collections {

template <>
struct __declspec(uuid("241709e6-4b79-44b4-827c-9bcb6025ebe6"))
    IObservableVector<GUID> : IObservableVector_impl<GUID> {};

template <>
struct __declspec(uuid("868ba4c1-7019-470b-b667-df34fa20efc6"))
    VectorChangedEventHandler<GUID> : VectorChangedEventHandler_impl<GUID> {};

template <>
struct __declspec(uuid("5a7b58a6-fbd4-4ef9-98fd-d700649cd32e"))
    IObservableVector<BluetoothLEManufacturerData*>
    : IObservableVector_impl<
          Internal::AggregateType<BluetoothLEManufacturerData*,
                                  IBluetoothLEManufacturerData*>> {};

template <>
struct __declspec(uuid("0a57dc65-0e06-46ff-9ff7-cf2390847d2a"))
    VectorChangedEventHandler<BluetoothLEManufacturerData*>
    : VectorChangedEventHandler_impl<
          Internal::AggregateType<BluetoothLEManufacturerData*,
                                  IBluetoothLEManufacturerData*>> {};

template <>
struct __declspec(uuid("eeec55af-8cd4-4935-aa43-31aa2c5567b9"))
    IObservableVector<BluetoothLEAdvertisementDataSection*>
    : IObservableVector_impl<
          Internal::AggregateType<BluetoothLEAdvertisementDataSection*,
                                  IBluetoothLEAdvertisementDataSection*>> {};

template <>
struct __declspec(uuid("978f98e6-b03c-41c8-a529-7781fd06f1e4"))
    VectorChangedEventHandler<BluetoothLEAdvertisementDataSection*>
    : VectorChangedEventHandler_impl<
          Internal::AggregateType<BluetoothLEAdvertisementDataSection*,
                                  IBluetoothLEAdvertisementDataSection*>> {};

}  // namespace Collections
}  // namespace Foundation
}  // namespace Windows
}  // namespace ABI

namespace device {

namespace {

std::vector<ComPtr<IBluetoothLEAdvertisementDataSection>> ToDataSections(
    const BluetoothDevice::ServiceDataMap& service_data) {
  std::vector<ComPtr<IBluetoothLEAdvertisementDataSection>> data_sections;
  data_sections.reserve(service_data.size());
  for (const auto& pair : service_data) {
    std::vector<uint8_t> data = pair.first.GetBytes();

    // Reverse the data as UUIDs are specified in little endian order in the
    // advertisement payload.
    std::reverse(data.begin(), data.end());

    // Append the actual service data and append a new data section.
    data.insert(data.end(), pair.second.begin(), pair.second.end());
    data_sections.push_back(
        Make<FakeBluetoothLEAdvertisementDataSectionWinrt>(std::move(data)));
  }

  return data_sections;
}

}  // namespace

FakeBluetoothLEAdvertisementWinrt::FakeBluetoothLEAdvertisementWinrt() =
    default;

FakeBluetoothLEAdvertisementWinrt::FakeBluetoothLEAdvertisementWinrt(
    std::optional<std::string> local_name,
    std::optional<uint8_t> flags,
    BluetoothDevice::UUIDList advertised_uuids,
    std::optional<int8_t> tx_power,
    BluetoothDevice::ServiceDataMap service_data,
    BluetoothDevice::ManufacturerDataMap manufacturer_data)
    : local_name_(std::move(local_name)),
      flags_(std::move(flags)),
      advertised_uuids_(std::move(advertised_uuids)),
      tx_power_(std::move(tx_power)),
      service_data_(std::move(service_data)),
      manufacturer_data_(std::move(manufacturer_data)) {}

FakeBluetoothLEAdvertisementWinrt::~FakeBluetoothLEAdvertisementWinrt() =
    default;

HRESULT FakeBluetoothLEAdvertisementWinrt::get_Flags(
    IReference<BluetoothLEAdvertisementFlags>** value) {
  // While non-intuitive, this matches production behavior. When a reference is
  // supposed to be empty, the pointer is set to null and S_OK is returned.
  if (!flags_) {
    *value = nullptr;
    return S_OK;
  }

  return Make<base::win::Reference<BluetoothLEAdvertisementFlags>>(
             static_cast<BluetoothLEAdvertisementFlags>(*flags_))
      .CopyTo(value);
}

HRESULT FakeBluetoothLEAdvertisementWinrt::put_Flags(
    IReference<BluetoothLEAdvertisementFlags>* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWinrt::get_LocalName(HSTRING* value) {
  if (!local_name_)
    return E_FAIL;

  *value = base::win::ScopedHString::Create(*local_name_).release();
  return S_OK;
}

HRESULT FakeBluetoothLEAdvertisementWinrt::put_LocalName(HSTRING value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWinrt::get_ServiceUuids(
    IVector<GUID>** value) {
  std::vector<GUID> guids;
  guids.reserve(advertised_uuids_.size());
  for (const auto& uuid : advertised_uuids_) {
    guids.emplace_back(
        BluetoothUUID::GetCanonicalValueAsGUID(uuid.canonical_value()));
  }

  return Make<base::win::Vector<GUID>>(std::move(guids)).CopyTo(value);
}

HRESULT FakeBluetoothLEAdvertisementWinrt::get_ManufacturerData(
    IVector<BluetoothLEManufacturerData*>** value) {
  std::vector<ComPtr<IBluetoothLEManufacturerData>> manufacturer_data;
  manufacturer_data.reserve(manufacturer_data_.size());
  for (const auto& pair : manufacturer_data_) {
    manufacturer_data.push_back(
        Make<FakeBluetoothLEManufacturerData>(pair.first, pair.second));
  }

  return Make<base::win::Vector<BluetoothLEManufacturerData*>>(
             std::move(manufacturer_data))
      .CopyTo(value);
}

HRESULT FakeBluetoothLEAdvertisementWinrt::get_DataSections(
    IVector<BluetoothLEAdvertisementDataSection*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWinrt::GetManufacturerDataByCompanyId(
    uint16_t company_id,
    IVectorView<BluetoothLEManufacturerData*>** data_list) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWinrt::GetSectionsByType(
    uint8_t type,
    IVectorView<BluetoothLEAdvertisementDataSection*>** section_list) {
  std::vector<ComPtr<IBluetoothLEAdvertisementDataSection>> data_sections;
  if (type == BluetoothDeviceWinrt::kTxPowerLevelDataSection && tx_power_) {
    data_sections.push_back(Make<FakeBluetoothLEAdvertisementDataSectionWinrt>(
        std::vector<uint8_t>({static_cast<uint8_t>(*tx_power_)})));
  }

  // For simplicity we only implement querying 128 Bit UUID Service Data.
  if (type == BluetoothDeviceWinrt::k128BitServiceDataSection)
    data_sections = ToDataSections(service_data_);

  return Make<base::win::Vector<BluetoothLEAdvertisementDataSection*>>(
             std::move(data_sections))
      ->GetView(section_list);
}

}  // namespace device
