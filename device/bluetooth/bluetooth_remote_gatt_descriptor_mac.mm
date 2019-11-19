// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_descriptor_mac.h"

#include "base/bind.h"
#import "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#import "device/bluetooth/bluetooth_adapter_mac.h"
#include "device/bluetooth/bluetooth_adapter_mac_metrics.h"
#import "device/bluetooth/bluetooth_remote_gatt_characteristic_mac.h"

using base::mac::ObjCCast;

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
    : gatt_characteristic_(characteristic),
      cb_descriptor_(descriptor, base::scoped_policy::RETAIN),
      value_read_or_write_in_progress_(false) {
  uuid_ = BluetoothAdapterMac::BluetoothUUIDWithCBUUID([cb_descriptor_ UUID]);
  identifier_ = base::SysNSStringToUTF8(
      [NSString stringWithFormat:@"%s-%p", uuid_.canonical_value().c_str(),
                                 cb_descriptor_.get()]);
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
  if (!read_value_callbacks_.first.is_null()) {
    std::move(read_value_callbacks_)
        .second.Run(BluetoothGattService::GATT_ERROR_FAILED);
  }
  if (!write_value_callbacks_.first.is_null()) {
    std::move(write_value_callbacks_)
        .second.Run(BluetoothGattService::GATT_ERROR_FAILED);
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
    ValueCallback callback,
    ErrorCallback error_callback) {
  if (value_read_or_write_in_progress_) {
    VLOG(1) << *this << ": Read failed, already in progress.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS));
    return;
  }
  VLOG(1) << *this << ": Read value.";
  value_read_or_write_in_progress_ = true;
  read_value_callbacks_ =
      std::make_pair(std::move(callback), std::move(error_callback));
  [GetCBPeripheral() readValueForDescriptor:cb_descriptor_];
}

void BluetoothRemoteGattDescriptorMac::WriteRemoteDescriptor(
    const std::vector<uint8_t>& value,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  if (value_read_or_write_in_progress_) {
    VLOG(1) << *this << ": Write failed, already in progress.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS));
    return;
  }
  VLOG(1) << *this << ": Write value.";
  value_read_or_write_in_progress_ = true;
  write_value_callbacks_ =
      std::make_pair(std::move(callback), std::move(error_callback));
  base::scoped_nsobject<NSData> nsdata_value(
      [[NSData alloc] initWithBytes:value.data() length:value.size()]);
  [GetCBPeripheral() writeValue:nsdata_value forDescriptor:GetCBDescriptor()];
}

void BluetoothRemoteGattDescriptorMac::DidUpdateValueForDescriptor(
    NSError* error) {
  if (!value_read_or_write_in_progress_) {
    VLOG(1) << *this << ": Value updated, no read in progress.";
    return;
  }
  std::pair<ValueCallback, ErrorCallback> callbacks;
  callbacks.swap(read_value_callbacks_);
  value_read_or_write_in_progress_ = false;
  RecordDidUpdateValueForDescriptorResult(error);
  if (error) {
    BluetoothGattService::GattErrorCode error_code =
        BluetoothDeviceMac::GetGattErrorCodeFromNSError(error);
    VLOG(1) << *this << ": Read value failed with error: "
            << BluetoothAdapterMac::String(error)
            << ", converted to error code: " << error_code;
    std::move(callbacks.second).Run(error_code);
    return;
  }
  VLOG(1) << *this << ": Value read.";
  value_ = VectorValueFromObjC([cb_descriptor_ value]);
  std::move(callbacks.first).Run(value_);
}

void BluetoothRemoteGattDescriptorMac::DidWriteValueForDescriptor(
    NSError* error) {
  if (!value_read_or_write_in_progress_) {
    VLOG(1) << *this << ": Value written, no write in progress.";
    return;
  }
  std::pair<base::OnceClosure, ErrorCallback> callbacks;
  callbacks.swap(write_value_callbacks_);
  value_read_or_write_in_progress_ = false;
  RecordDidWriteValueForDescriptorResult(error);
  if (error) {
    BluetoothGattService::GattErrorCode error_code =
        BluetoothDeviceMac::GetGattErrorCodeFromNSError(error);
    VLOG(1) << *this << ": Write value failed with error: "
            << BluetoothAdapterMac::String(error)
            << ", converted to error code: " << error_code;
    std::move(callbacks.second).Run(error_code);
    return;
  }
  VLOG(1) << *this << ": Value written.";
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
