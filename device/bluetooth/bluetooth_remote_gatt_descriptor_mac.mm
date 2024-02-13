// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_descriptor_mac.h"

#import "base/apple/foundation_util.h"
#include "base/functional/bind.h"
#include "base/strings/sys_string_conversions.h"
#import "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "device/bluetooth/bluetooth_low_energy_adapter_apple.h"
#import "device/bluetooth/bluetooth_remote_gatt_characteristic_mac.h"

using base::apple::ObjCCast;

namespace device {

std::vector<uint8_t> VectorValueFromObjC(id objc_value) {
  // According to
  // https://developer.apple.com/reference/corebluetooth/cbdescriptor some
  // descriptor values can be NSData, NSString or NSNumber.
  std::vector<uint8_t> value;
  NSData* data = ObjCCast<NSData>(objc_value);
  NSString* as_string = ObjCCast<NSString>(objc_value);
  NSNumber* as_number = ObjCCast<NSNumber>(objc_value);

  if (!data && !as_string && as_number) {
    unsigned short descriptor = [as_number shortValue];
    data = [NSData dataWithBytes:&descriptor length:sizeof(descriptor)];
  }

  if (!data && as_string)
    data = [as_string dataUsingEncoding:NSUTF8StringEncoding];

  if (data) {
    value.resize([data length]);
    [data getBytes:value.data() length:value.size()];
  } else {
    LOG(WARNING) << "Unexpected value: "
                 << base::SysNSStringToUTF8([objc_value description]);
  }
  return value;
}

BluetoothRemoteGattDescriptorMac::BluetoothRemoteGattDescriptorMac(
    BluetoothRemoteGattCharacteristicMac* characteristic,
    CBDescriptor* descriptor)
    : gatt_characteristic_(characteristic), cb_descriptor_(descriptor) {
  uuid_ = BluetoothLowEnergyAdapterApple::BluetoothUUIDWithCBUUID(
      [cb_descriptor_ UUID]);
  identifier_ = base::SysNSStringToUTF8(
      [NSString stringWithFormat:@"%s-%p", uuid_.canonical_value().c_str(),
                                 cb_descriptor_]);
}

std::string BluetoothRemoteGattDescriptorMac::GetIdentifier() const {
  return identifier_;
}

BluetoothUUID BluetoothRemoteGattDescriptorMac::GetUUID() const {
  return uuid_;
}

BluetoothGattCharacteristic::Permissions
BluetoothRemoteGattDescriptorMac::GetPermissions() const {
  NOTIMPLEMENTED();
  return BluetoothGattCharacteristic::PERMISSION_NONE;
}

const std::vector<uint8_t>& BluetoothRemoteGattDescriptorMac::GetValue() const {
  return value_;
}

BluetoothRemoteGattDescriptorMac::~BluetoothRemoteGattDescriptorMac() {
  destructor_called_ = true;
  if (HasPendingRead()) {
    std::move(read_value_callback_)
        .Run(BluetoothGattService::GattErrorCode::kFailed,
             /*value=*/std::vector<uint8_t>());
  }
  if (HasPendingWrite()) {
    std::move(write_value_callbacks_)
        .second.Run(BluetoothGattService::GattErrorCode::kFailed);
  }
}

BluetoothRemoteGattCharacteristic*
BluetoothRemoteGattDescriptorMac::GetCharacteristic() const {
  return static_cast<BluetoothRemoteGattCharacteristic*>(gatt_characteristic_);
}

// Sends a read request to a remote characteristic descriptor to read its
// value. |callback| is called to return the read value on success and
// |error_callback| is called for failures.
void BluetoothRemoteGattDescriptorMac::ReadRemoteDescriptor(
    ValueCallback callback) {
  if (destructor_called_ || HasPendingRead() || HasPendingWrite()) {
    DVLOG(1) << *this << ": Read failed, already in progress.";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       BluetoothGattService::GattErrorCode::kInProgress,
                       /*value=*/std::vector<uint8_t>()));
    return;
  }
  DVLOG(1) << *this << ": Read value.";
  read_value_callback_ = std::move(callback);
  [GetCBPeripheral() readValueForDescriptor:cb_descriptor_];
}

void BluetoothRemoteGattDescriptorMac::WriteRemoteDescriptor(
    const std::vector<uint8_t>& value,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  if (destructor_called_ || HasPendingRead() || HasPendingWrite()) {
    DVLOG(1) << *this << ": Write failed, already in progress.";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       BluetoothGattService::GattErrorCode::kInProgress));
    return;
  }
  DVLOG(1) << *this << ": Write value.";
  write_value_callbacks_ =
      std::make_pair(std::move(callback), std::move(error_callback));
  NSData* nsdata_value = [[NSData alloc] initWithBytes:value.data()
                                                length:value.size()];
  [GetCBPeripheral() writeValue:nsdata_value forDescriptor:GetCBDescriptor()];
}

void BluetoothRemoteGattDescriptorMac::DidUpdateValueForDescriptor(
    NSError* error) {
  if (!HasPendingRead()) {
    DVLOG(1) << *this << ": Value updated, no read in progress.";
    return;
  }
  if (error) {
    BluetoothGattService::GattErrorCode error_code =
        BluetoothDeviceMac::GetGattErrorCodeFromNSError(error);
    DVLOG(1) << *this << ": Read value failed with error: "
             << BluetoothLowEnergyAdapterApple::String(error)
             << ", converted to error code: " << static_cast<int>(error_code);
    std::move(read_value_callback_)
        .Run(error_code,
             /*value=*/std::vector<uint8_t>());
    return;
  }
  DVLOG(1) << *this << ": Value read.";
  value_ = VectorValueFromObjC([cb_descriptor_ value]);
  std::move(read_value_callback_).Run(/*error_code=*/std::nullopt, value_);
}

void BluetoothRemoteGattDescriptorMac::DidWriteValueForDescriptor(
    NSError* error) {
  if (!HasPendingWrite()) {
    DVLOG(1) << *this << ": Value written, no write in progress.";
    return;
  }
  std::pair<base::OnceClosure, ErrorCallback> callbacks;
  callbacks.swap(write_value_callbacks_);
  if (error) {
    BluetoothGattService::GattErrorCode error_code =
        BluetoothDeviceMac::GetGattErrorCodeFromNSError(error);
    DVLOG(1) << *this << ": Write value failed with error: "
             << BluetoothLowEnergyAdapterApple::String(error)
             << ", converted to error code: " << static_cast<int>(error_code);
    std::move(callbacks.second).Run(error_code);
    return;
  }
  DVLOG(1) << *this << ": Value written.";
  std::move(callbacks.first).Run();
}

CBPeripheral* BluetoothRemoteGattDescriptorMac::GetCBPeripheral() const {
  return gatt_characteristic_->GetCBPeripheral();
}

CBDescriptor* BluetoothRemoteGattDescriptorMac::GetCBDescriptor() const {
  return cb_descriptor_;
}

DEVICE_BLUETOOTH_EXPORT std::ostream& operator<<(
    std::ostream& out,
    const BluetoothRemoteGattDescriptorMac& descriptor) {
  const BluetoothRemoteGattCharacteristicMac* characteristic_mac =
      static_cast<const BluetoothRemoteGattCharacteristicMac*>(
          descriptor.GetCharacteristic());
  return out << "<BluetoothRemoteGattDescriptorMac "
             << descriptor.GetUUID().canonical_value() << "/" << &descriptor
             << ", characteristic: "
             << characteristic_mac->GetUUID().canonical_value() << "/"
             << characteristic_mac << ">";
}

}  // namespace device.
