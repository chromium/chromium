// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_gatt_device_services_result_winrt.h"

#include <windows.foundation.collections.h>
#include <wrl/client.h>

#include <utility>

#include "base/win/vector.h"
#include "device/bluetooth/test/fake_gatt_device_service_winrt.h"

namespace {

using ABI::Windows::Foundation::Collections::IVectorView;
using ABI::Windows::Foundation::IReference;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDeviceService;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus_Success;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattDeviceService;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

}  // namespace

// Note: As UWP does not provide GattDeviceService specializations for
// IObservableVector, VectorChangedEventHandler and IVector we need to supply
// our own. UUIDs were generated using `uuidgen`.
namespace ABI {
namespace Windows {
namespace Foundation {
namespace Collections {

template <>
struct __declspec(uuid("eeec55af-8cd4-4935-aa43-31aa2c5567b9"))
    IObservableVector<GattDeviceService*>
    : IObservableVector_impl<
          Internal::AggregateType<GattDeviceService*, IGattDeviceService*>> {};

template <>
struct __declspec(uuid("657a5b0f-aae7-4873-92f4-3532e6fb3a37"))
    VectorChangedEventHandler<GattDeviceService*>
    : VectorChangedEventHandler_impl<
          Internal::AggregateType<GattDeviceService*, IGattDeviceService*>> {};

template <>
struct __declspec(
    uuid("a400ae0d-f74d-4e12-9014-77d3a6997ecb")) IVector<GattDeviceService*>
    : IVector_impl<
          Internal::AggregateType<GattDeviceService*, IGattDeviceService*>> {};

}  // namespace Collections
}  // namespace Foundation
}  // namespace Windows
}  // namespace ABI

namespace device {

FakeGattDeviceServicesResultWinrt::FakeGattDeviceServicesResultWinrt(
    ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
        GattCommunicationStatus status)
    : status_(status) {}

FakeGattDeviceServicesResultWinrt::FakeGattDeviceServicesResultWinrt(
    const std::vector<ComPtr<FakeGattDeviceServiceWinrt>>& fake_services)
    : status_(GattCommunicationStatus_Success),
      services_(fake_services.begin(), fake_services.end()) {}

FakeGattDeviceServicesResultWinrt::~FakeGattDeviceServicesResultWinrt() =
    default;

HRESULT FakeGattDeviceServicesResultWinrt::get_Status(
    GattCommunicationStatus* value) {
  *value = status_;
  return S_OK;
}

HRESULT FakeGattDeviceServicesResultWinrt::get_ProtocolError(
    IReference<uint8_t>** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServicesResultWinrt::get_Services(
    IVectorView<GattDeviceService*>** value) {
  return Make<base::win::Vector<GattDeviceService*>>(services_)->GetView(value);
}

}  // namespace device
