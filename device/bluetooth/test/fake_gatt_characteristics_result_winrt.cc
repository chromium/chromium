// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_gatt_characteristics_result_winrt.h"

#include "base/win/vector.h"
#include "device/bluetooth/test/fake_gatt_characteristic_winrt.h"

namespace {

using ABI::Windows::Foundation::Collections::IVectorView;
using ABI::Windows::Foundation::IReference;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCharacteristic;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus_Success;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattCharacteristic;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

}  // namespace

// Note: As UWP does not provide GattCharacteristic specializations for
// IObservableVector, VectorChangedEventHandler and IVector we need to supply
// our own. UUIDs were generated using `uuidgen`.
namespace ABI {
namespace Windows {
namespace Foundation {
namespace Collections {

template <>
struct __declspec(uuid("423c3781-7383-4e38-ad42-01b0d9ee160e"))
    IObservableVector<GattCharacteristic*>
    : IObservableVector_impl<
          Internal::AggregateType<GattCharacteristic*, IGattCharacteristic*>> {
};

template <>
struct __declspec(uuid("b334a2e8-90d1-48fc-8893-aecea6b23202"))
    VectorChangedEventHandler<GattCharacteristic*>
    : VectorChangedEventHandler_impl<
          Internal::AggregateType<GattCharacteristic*, IGattCharacteristic*>> {
};

template <>
struct __declspec(
    uuid("072c852b-da31-4d46-884d-3a3a2157c986")) IVector<GattCharacteristic*>
    : IVector_impl<
          Internal::AggregateType<GattCharacteristic*, IGattCharacteristic*>> {
};

}  // namespace Collections
}  // namespace Foundation
}  // namespace Windows
}  // namespace ABI

namespace device {

FakeGattCharacteristicsResultWinrt::FakeGattCharacteristicsResultWinrt(
    const std::vector<ComPtr<FakeGattCharacteristicWinrt>>&
        fake_characteristics)
    : characteristics_(fake_characteristics.begin(),
                       fake_characteristics.end()) {}

FakeGattCharacteristicsResultWinrt::~FakeGattCharacteristicsResultWinrt() =
    default;

HRESULT FakeGattCharacteristicsResultWinrt::get_Status(
    GattCommunicationStatus* value) {
  *value = GattCommunicationStatus_Success;
  return S_OK;
}

HRESULT FakeGattCharacteristicsResultWinrt::get_ProtocolError(
    IReference<uint8_t>** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicsResultWinrt::get_Characteristics(
    IVectorView<GattCharacteristic*>** value) {
  return Make<base::win::Vector<GattCharacteristic*>>(characteristics_)
      ->GetView(value);
}

}  // namespace device
