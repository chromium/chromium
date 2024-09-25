// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_service_winrt.h"

#include <windows.foundation.collections.h>

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_discoverer_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattDeviceService;
using Microsoft::WRL::ComPtr;

}  // namespace

// static
std::unique_ptr<BluetoothRemoteGattServiceWinrt>
BluetoothRemoteGattServiceWinrt::Create(
    BluetoothDevice* device,
    ComPtr<IGattDeviceService> gatt_service) {
  DCHECK(gatt_service);
  GUID guid;
  HRESULT hr = gatt_service->get_Uuid(&guid);
  if (FAILED(hr)) {
    DVLOG(2) << "Getting UUID failed: " << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  uint16_t attribute_handle;
  hr = gatt_service->get_AttributeHandle(&attribute_handle);
  if (FAILED(hr)) {
    DVLOG(2) << "Getting AttributeHandle failed: "
             << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  return base::WrapUnique(new BluetoothRemoteGattServiceWinrt(
      device, std::move(gatt_service), BluetoothUUID(guid), attribute_handle));
}

BluetoothRemoteGattServiceWinrt::~BluetoothRemoteGattServiceWinrt() = default;

std::string BluetoothRemoteGattServiceWinrt::GetIdentifier() const {
  return identifier_;
}

BluetoothUUID BluetoothRemoteGattServiceWinrt::GetUUID() const {
  return uuid_;
}

bool BluetoothRemoteGattServiceWinrt::IsPrimary() const {
  return true;
}

BluetoothDevice* BluetoothRemoteGattServiceWinrt::GetDevice() const {
  return device_;
}

std::vector<BluetoothRemoteGattService*>
BluetoothRemoteGattServiceWinrt::GetIncludedServices() const {
  NOTIMPLEMENTED();
  return {};
}

void BluetoothRemoteGattServiceWinrt::UpdateCharacteristics(
    BluetoothGattDiscovererWinrt* gatt_discoverer) {
  const auto* gatt_characteristics =
      gatt_discoverer->GetCharacteristics(attribute_handle_);
  DCHECK(gatt_characteristics);

  // Instead of clearing out |characteristics_| and creating each characteristic
  // from scratch, we create a new map and move already existing characteristics
  // into it in order to preserve pointer stability.
  CharacteristicMap characteristics;
  for (const auto& gatt_characteristic : *gatt_characteristics) {
    auto characteristic = BluetoothRemoteGattCharacteristicWinrt::Create(
        this, gatt_characteristic.Get());
    if (!characteristic)
      continue;

    std::string identifier = characteristic->GetIdentifier();
    auto iter = characteristics_.find(identifier);
    if (iter != characteristics_.end()) {
      iter = characteristics.emplace(std::move(*iter)).first;
    } else {
      iter = characteristics
                 .emplace(std::move(identifier), std::move(characteristic))
                 .first;
    }

    static_cast<BluetoothRemoteGattCharacteristicWinrt*>(iter->second.get())
        ->UpdateDescriptors(gatt_discoverer);
  }

  std::swap(characteristics, characteristics_);
  SetDiscoveryComplete(true);
}

IGattDeviceService*
BluetoothRemoteGattServiceWinrt::GetDeviceServiceForTesting() {
  return gatt_service_.Get();
}

// static
uint8_t BluetoothRemoteGattServiceWinrt::ToProtocolError(
    GattErrorCode error_code) {
  switch (error_code) {
    case GattErrorCode::kUnknown:
      return 0xF0;
    case GattErrorCode::kFailed:
      return 0x01;
    case GattErrorCode::kInProgress:
      return 0x09;
    case GattErrorCode::kInvalidLength:
      return 0x0D;
    case GattErrorCode::kNotPermitted:
      return 0x02;
    case GattErrorCode::kNotAuthorized:
      return 0x08;
    case GattErrorCode::kNotPaired:
      return 0x0F;
    case GattErrorCode::kNotSupported:
      return 0x06;
  }

  NOTREACHED();
}

BluetoothRemoteGattServiceWinrt::BluetoothRemoteGattServiceWinrt(
    BluetoothDevice* device,
    ComPtr<IGattDeviceService> gatt_service,
    BluetoothUUID uuid,
    uint16_t attribute_handle)
    : device_(device),
      gatt_service_(std::move(gatt_service)),
      uuid_(std::move(uuid)),
      attribute_handle_(attribute_handle),
      identifier_(base::StringPrintf("%s/%s_%04x",
                                     device_->GetIdentifier().c_str(),
                                     uuid_.value().c_str(),
                                     attribute_handle)) {}
}  // namespace device
