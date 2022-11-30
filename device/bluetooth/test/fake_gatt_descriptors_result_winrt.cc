// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_gatt_descriptors_result_winrt.h"

#include "base/win/vector.h"
#include "device/bluetooth/test/fake_gatt_descriptor_winrt.h"

namespace {

using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus_Success;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::GattDescriptor;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattDescriptor;
using ABI::Windows::Foundation::Collections::IVectorView;
using ABI::Windows::Foundation::IReference;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

}  // namespace

// Note: As UWP does not provide GattDescriptor specializations for
// IObservableVector, VectorChangedEventHandler and IVector we need to supply
// our own. UUIDs were generated using `uuidgen`.
namespace ABI {
namespace Windows {
namespace Foundation {
namespace Collections {

template <>
struct __declspec(uuid("b259bb8d-2a87-44f6-9f9c-321cb98b7750"))
    IObservableVector<GattDescriptor*>
    : IObservableVector_impl<
          Internal::AggregateType<GattDescriptor*, IGattDescriptor*>> {};

template <>
struct __declspec(uuid("39bc2e35-9a9a-4f93-ba00-7caaf965457e"))
    VectorChangedEventHandler<GattDescriptor*>
    : VectorChangedEventHandler_impl<
          Internal::AggregateType<GattDescriptor*, IGattDescriptor*>> {};

template <>
struct __declspec(
    uuid("1865abfa-a793-4b20-910c-f43e3fd12c3c")) IVector<GattDescriptor*>
    : IVector_impl<Internal::AggregateType<GattDescriptor*, IGattDescriptor*>> {
};

}  // namespace Collections
}  // namespace Foundation
}  // namespace Windows
}  // namespace ABI

namespace device {

FakeGattDescriptorsResultWinrt::FakeGattDescriptorsResultWinrt(
    const std::vector<ComPtr<FakeGattDescriptorWinrt>>& fake_descriptors)
    : descriptors_(fake_descriptors.begin(), fake_descriptors.end()) {}

FakeGattDescriptorsResultWinrt::~FakeGattDescriptorsResultWinrt() = default;

HRESULT FakeGattDescriptorsResultWinrt::get_Status(
    GattCommunicationStatus* value) {
  *value = GattCommunicationStatus_Success;
  return S_OK;
}

HRESULT FakeGattDescriptorsResultWinrt::get_ProtocolError(
    IReference<uint8_t>** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDescriptorsResultWinrt::get_Descriptors(
    IVectorView<GattDescriptor*>** value) {
  return Make<base::win::Vector<GattDescriptor*>>(descriptors_)->GetView(value);
}

}  // namespace device
