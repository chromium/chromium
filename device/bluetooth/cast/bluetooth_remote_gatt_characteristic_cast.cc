// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/cast/bluetooth_remote_gatt_characteristic_cast.h"

#include <memory>

#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/device/bluetooth/le/remote_characteristic.h"
#include "chromecast/device/bluetooth/le/remote_descriptor.h"
#include "chromecast/public/bluetooth/gatt.h"
#include "device/bluetooth/cast/bluetooth_remote_gatt_descriptor_cast.h"
#include "device/bluetooth/cast/bluetooth_remote_gatt_service_cast.h"
#include "device/bluetooth/cast/bluetooth_utils.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace device {
namespace {

BluetoothGattCharacteristic::Permissions ConvertPermissions(
    chromecast::bluetooth_v2_shlib::Gatt::Permissions input) {
  BluetoothGattCharacteristic::Permissions output =
      BluetoothGattCharacteristic::PERMISSION_NONE;
  if (input & chromecast::bluetooth_v2_shlib::Gatt::PERMISSION_READ)
    output |= BluetoothGattCharacteristic::PERMISSION_READ;
  if (input & chromecast::bluetooth_v2_shlib::Gatt::PERMISSION_READ_ENCRYPTED)
    output |= BluetoothGattCharacteristic::PERMISSION_READ_ENCRYPTED;
  if (input & chromecast::bluetooth_v2_shlib::Gatt::PERMISSION_WRITE)
    output |= BluetoothGattCharacteristic::PERMISSION_WRITE;
  if (input & chromecast::bluetooth_v2_shlib::Gatt::PERMISSION_WRITE_ENCRYPTED)
    output |= BluetoothGattCharacteristic::PERMISSION_WRITE_ENCRYPTED;

  // NOTE(slan): Determine the proper mapping for these.
  // if (input & chromecast::bluetooth_v2_shlib::PERMISSION_READ_ENCRYPTED_MITM)
  //   output |= BluetoothGattCharacteristic::PERMISSION_READ_ENCRYPTED_MITM;
  // if (input &
  // chromecast::bluetooth_v2_shlib::PERMISSION_WRITE_ENCRYPTED_MITM)
  //   output |= BluetoothGattCharacteristic::PERMISSION_WRITE_ENCRYPTED_MITM;
  // if (input & chromecast::bluetooth_v2_shlib::PERMISSION_WRITE_SIGNED)
  //   output |= BluetoothGattCharacteristic::PERMISSION_WRITE_SIGNED;
  // if (input & chromecast::bluetooth_v2_shlib::PERMISSION_WRITE_SIGNED_MITM)
  //   output |= BluetoothGattCharacteristic::PERMISSION_WRITE_SIGNED_MITM;

  return output;
}

BluetoothGattCharacteristic::Properties ConvertProperties(
    chromecast::bluetooth_v2_shlib::Gatt::Properties input) {
  BluetoothGattCharacteristic::Properties output =
      BluetoothGattCharacteristic::PROPERTY_NONE;
  if (input & chromecast::bluetooth_v2_shlib::Gatt::PROPERTY_BROADCAST)
    output |= BluetoothGattCharacteristic::PROPERTY_BROADCAST;
  if (input & chromecast::bluetooth_v2_shlib::Gatt::PROPERTY_READ)
    output |= BluetoothGattCharacteristic::PROPERTY_READ;
  if (input & chromecast::bluetooth_v2_shlib::Gatt::PROPERTY_WRITE_NO_RESPONSE)
    output |= BluetoothGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE;
  if (input & chromecast::bluetooth_v2_shlib::Gatt::PROPERTY_WRITE)
    output |= BluetoothGattCharacteristic::PROPERTY_WRITE;
  if (input & chromecast::bluetooth_v2_shlib::Gatt::PROPERTY_NOTIFY)
    output |= BluetoothGattCharacteristic::PROPERTY_NOTIFY;
  if (input & chromecast::bluetooth_v2_shlib::Gatt::PROPERTY_INDICATE)
    output |= BluetoothGattCharacteristic::PROPERTY_INDICATE;
  if (input & chromecast::bluetooth_v2_shlib::Gatt::PROPERTY_SIGNED_WRITE)
    output |= BluetoothGattCharacteristic::PROPERTY_AUTHENTICATED_SIGNED_WRITES;
  if (input & chromecast::bluetooth_v2_shlib::Gatt::PROPERTY_EXTENDED_PROPS)
    output |= BluetoothGattCharacteristic::PROPERTY_EXTENDED_PROPERTIES;

  return output;
}

// Called back when subscribing or unsubscribing to a remote characteristic.
// If |success| is true, run |callback|. Otherwise run |error_callback|.
void OnSubscribeOrUnsubscribe(
    base::OnceClosure callback,
    BluetoothGattCharacteristic::ErrorCallback error_callback,
    bool success) {
  if (success)
    std::move(callback).Run();
  else
    std::move(error_callback).Run(BluetoothGattService::GattErrorCode::kFailed);
}

}  // namespace

BluetoothRemoteGattCharacteristicCast::BluetoothRemoteGattCharacteristicCast(
    BluetoothRemoteGattServiceCast* service,
    scoped_refptr<chromecast::bluetooth::RemoteCharacteristic> characteristic)
    : service_(service),
      remote_characteristic_(std::move(characteristic)),
      weak_factory_(this) {
  auto descriptors = remote_characteristic_->GetDescriptors();
  descriptors_.reserve(descriptors.size());
  for (const auto& descriptor : descriptors) {
    AddDescriptor(
        std::make_unique<BluetoothRemoteGattDescriptorCast>(this, descriptor));
  }
}

BluetoothRemoteGattCharacteristicCast::
    ~BluetoothRemoteGattCharacteristicCast() {}

std::string BluetoothRemoteGattCharacteristicCast::GetIdentifier() const {
  return GetUUID().canonical_value();
}

BluetoothUUID BluetoothRemoteGattCharacteristicCast::GetUUID() const {
  return UuidToBluetoothUUID(remote_characteristic_->uuid());
}

BluetoothGattCharacteristic::Properties
BluetoothRemoteGattCharacteristicCast::GetProperties() const {
  return ConvertProperties(remote_characteristic_->properties());
}

BluetoothGattCharacteristic::Permissions
BluetoothRemoteGattCharacteristicCast::GetPermissions() const {
  return ConvertPermissions(remote_characteristic_->permissions());
}

const std::vector<uint8_t>& BluetoothRemoteGattCharacteristicCast::GetValue()
    const {
  return value_;
}

BluetoothRemoteGattService* BluetoothRemoteGattCharacteristicCast::GetService()
    const {
  return service_;
}

void BluetoothRemoteGattCharacteristicCast::ReadRemoteCharacteristic(
    ValueCallback callback) {
  remote_characteristic_->Read(base::BindOnce(
      &BluetoothRemoteGattCharacteristicCast::OnReadRemoteCharacteristic,
      weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BluetoothRemoteGattCharacteristicCast::WriteRemoteCharacteristic(
    const std::vector<uint8_t>& value,
    WriteType write_type,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  using ChromecastWriteType = chromecast::bluetooth_v2_shlib::Gatt::WriteType;

  ChromecastWriteType chromecast_write_type;
  switch (write_type) {
    case WriteType::kWithResponse:
      chromecast_write_type = ChromecastWriteType::WRITE_TYPE_DEFAULT;
      break;
    case WriteType::kWithoutResponse:
      chromecast_write_type = ChromecastWriteType::WRITE_TYPE_NO_RESPONSE;
      break;
  }

  remote_characteristic_->WriteAuth(
      chromecast::bluetooth_v2_shlib::Gatt::Client::AUTH_REQ_NONE,
      chromecast_write_type, value,
      base::BindOnce(
          &BluetoothRemoteGattCharacteristicCast::OnWriteRemoteCharacteristic,
          weak_factory_.GetWeakPtr(), value, std::move(callback),
          std::move(error_callback)));
}

void BluetoothRemoteGattCharacteristicCast::DeprecatedWriteRemoteCharacteristic(
    const std::vector<uint8_t>& value,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  remote_characteristic_->Write(
      value,
      base::BindOnce(
          &BluetoothRemoteGattCharacteristicCast::OnWriteRemoteCharacteristic,
          weak_factory_.GetWeakPtr(), value, std::move(callback),
          std::move(error_callback)));
}

void BluetoothRemoteGattCharacteristicCast::SubscribeToNotifications(
    [[maybe_unused]] BluetoothRemoteGattDescriptor* ccc_descriptor,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  DVLOG(2) << __func__ << " " << GetIdentifier();

  // |remote_characteristic_| exposes a method which writes the CCCD after
  // subscribing the GATT client to the notification. This is syntactically
  // nicer and saves us a thread-hop, so we ignore |ccc_descriptor|.

  remote_characteristic_->SetRegisterNotification(
      true, base::BindOnce(&OnSubscribeOrUnsubscribe, std::move(callback),
                           std::move(error_callback)));
}

void BluetoothRemoteGattCharacteristicCast::UnsubscribeFromNotifications(
    [[maybe_unused]] BluetoothRemoteGattDescriptor* ccc_descriptor,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  DVLOG(2) << __func__ << " " << GetIdentifier();

  // |remote_characteristic_| exposes a method which writes the CCCD after
  // unsubscribing the GATT client from the notification. This is syntactically
  // nicer and saves us a thread-hop, so we ignore |ccc_descriptor|.

  remote_characteristic_->SetRegisterNotification(
      false, base::BindOnce(&OnSubscribeOrUnsubscribe, std::move(callback),
                            std::move(error_callback)));
}

void BluetoothRemoteGattCharacteristicCast::OnReadRemoteCharacteristic(
    ValueCallback callback,
    bool success,
    const std::vector<uint8_t>& result) {
  if (success) {
    value_ = result;
    std::move(callback).Run(/*error_code=*/std::nullopt, result);
    return;
  }
  std::move(callback).Run(BluetoothGattService::GattErrorCode::kFailed,
                          /*value=*/std::vector<uint8_t>());
}

void BluetoothRemoteGattCharacteristicCast::OnWriteRemoteCharacteristic(
    const std::vector<uint8_t>& written_value,
    base::OnceClosure callback,
    ErrorCallback error_callback,
    bool success) {
  if (success) {
    value_ = written_value;
    std::move(callback).Run();
    return;
  }
  std::move(error_callback).Run(BluetoothGattService::GattErrorCode::kFailed);
}

}  // namespace device
