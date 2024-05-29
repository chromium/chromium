// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_service_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "device/bluetooth/bluetooth_adapter_android.h"
#include "device/bluetooth/bluetooth_device_android.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "device/bluetooth/jni_headers/ChromeBluetoothRemoteGattService_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;

namespace device {

// static
std::unique_ptr<BluetoothRemoteGattServiceAndroid>
BluetoothRemoteGattServiceAndroid::Create(
    BluetoothAdapterAndroid* adapter,
    BluetoothDeviceAndroid* device,
    const JavaRef<jobject>&
        bluetooth_gatt_service_wrapper,  // BluetoothGattServiceWrapper
    const std::string& instance_id,
    const JavaRef<jobject>& chrome_bluetooth_device) {  // ChromeBluetoothDevice
  std::unique_ptr<BluetoothRemoteGattServiceAndroid> service(
      new BluetoothRemoteGattServiceAndroid(adapter, device, instance_id));

  JNIEnv* env = AttachCurrentThread();
  service->j_service_.Reset(Java_ChromeBluetoothRemoteGattService_create(
      env, reinterpret_cast<intptr_t>(service.get()),
      bluetooth_gatt_service_wrapper,
      base::android::ConvertUTF8ToJavaString(env, instance_id),
      chrome_bluetooth_device));

  return service;
}

BluetoothRemoteGattServiceAndroid::~BluetoothRemoteGattServiceAndroid() {
  Java_ChromeBluetoothRemoteGattService_onBluetoothRemoteGattServiceAndroidDestruction(
      AttachCurrentThread(), j_service_);
}

base::android::ScopedJavaLocalRef<jobject>
BluetoothRemoteGattServiceAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(j_service_);
}

// static
BluetoothGattService::GattErrorCode
BluetoothRemoteGattServiceAndroid::GetGattErrorCode(int bluetooth_gatt_code) {
  DCHECK(bluetooth_gatt_code != 0) << "Only errors valid. 0 == GATT_SUCCESS.";

  // TODO(scheib) Create new BluetoothGattService::GattErrorCode enums for
  // android values not yet represented. http://crbug.com/548498
  switch (bluetooth_gatt_code) {  // android.bluetooth.BluetoothGatt values:
    case 0x00000101:              // GATT_FAILURE
      return GattErrorCode::kFailed;
    case 0x0000000d:  // GATT_INVALID_ATTRIBUTE_LENGTH
      return GattErrorCode::kInvalidLength;
    case 0x00000002:  // GATT_READ_NOT_PERMITTED
      return GattErrorCode::kNotPermitted;
    case 0x00000006:  // GATT_REQUEST_NOT_SUPPORTED
      return GattErrorCode::kNotSupported;
    case 0x00000003:  // GATT_WRITE_NOT_PERMITTED
      return GattErrorCode::kNotPermitted;
    default:
      DVLOG(1) << "Unhandled status: " << bluetooth_gatt_code;
      return BluetoothGattService::GattErrorCode::kUnknown;
  }
}

// static
int BluetoothRemoteGattServiceAndroid::GetAndroidErrorCode(
    BluetoothGattService::GattErrorCode error_code) {
  // TODO(scheib) Create new BluetoothGattService::GattErrorCode enums for
  // android values not yet represented. http://crbug.com/548498
  switch (error_code) {  // Return values from android.bluetooth.BluetoothGatt:
    case GattErrorCode::kUnknown:
      return 0x00000101;  // GATT_FAILURE. No good match.
    case GattErrorCode::kFailed:
      return 0x00000101;  // GATT_FAILURE
    case GattErrorCode::kInProgress:
      return 0x00000101;  // GATT_FAILURE. No good match.
    case GattErrorCode::kInvalidLength:
      return 0x0000000d;  // GATT_INVALID_ATTRIBUTE_LENGTH
    case GattErrorCode::kNotPermitted:
      // Can't distinguish between:
      // 0x00000002:  // GATT_READ_NOT_PERMITTED
      // 0x00000003:  // GATT_WRITE_NOT_PERMITTED
      return 0x00000101;  // GATT_FAILURE. No good match.
    case GattErrorCode::kNotAuthorized:
      return 0x00000101;  // GATT_FAILURE. No good match.
    case GattErrorCode::kNotPaired:
      return 0x00000101;  // GATT_FAILURE. No good match.
    case GattErrorCode::kNotSupported:
      return 0x00000006;  // GATT_REQUEST_NOT_SUPPORTED
  }
  DVLOG(1) << "Unhandled error_code: " << static_cast<int>(error_code);
  return 0x00000101;  // GATT_FAILURE. No good match.
}

std::string BluetoothRemoteGattServiceAndroid::GetIdentifier() const {
  return instance_id_;
}

device::BluetoothUUID BluetoothRemoteGattServiceAndroid::GetUUID() const {
  return device::BluetoothUUID(base::android::ConvertJavaStringToUTF8(
      Java_ChromeBluetoothRemoteGattService_getUUID(AttachCurrentThread(),
                                                    j_service_)));
}

bool BluetoothRemoteGattServiceAndroid::IsPrimary() const {
  NOTIMPLEMENTED();
  return true;
}

device::BluetoothDevice* BluetoothRemoteGattServiceAndroid::GetDevice() const {
  return device_;
}

std::vector<device::BluetoothRemoteGattCharacteristic*>
BluetoothRemoteGattServiceAndroid::GetCharacteristics() const {
  EnsureCharacteristicsCreated();
  return BluetoothRemoteGattService::GetCharacteristics();
}

std::vector<device::BluetoothRemoteGattService*>
BluetoothRemoteGattServiceAndroid::GetIncludedServices() const {
  NOTIMPLEMENTED();
  return std::vector<device::BluetoothRemoteGattService*>();
}

device::BluetoothRemoteGattCharacteristic*
BluetoothRemoteGattServiceAndroid::GetCharacteristic(
    const std::string& identifier) const {
  EnsureCharacteristicsCreated();
  return BluetoothRemoteGattService::GetCharacteristic(identifier);
}

std::vector<BluetoothRemoteGattCharacteristic*>
BluetoothRemoteGattServiceAndroid::GetCharacteristicsByUUID(
    const BluetoothUUID& characteristic_uuid) const {
  EnsureCharacteristicsCreated();
  return BluetoothRemoteGattService::GetCharacteristicsByUUID(
      characteristic_uuid);
}

bool BluetoothRemoteGattServiceAndroid::IsDiscoveryComplete() const {
  // Not used on Android, because Android sends an event when service discovery
  // is complete for the entire device.
  NOTIMPLEMENTED();
  return true;
}

void BluetoothRemoteGattServiceAndroid::SetDiscoveryComplete(bool complete) {
  // Not used on Android, because Android sends an event when service discovery
  // is complete for the entire device.
  NOTIMPLEMENTED();
}

void BluetoothRemoteGattServiceAndroid::CreateGattRemoteCharacteristic(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller,
    const JavaParamRef<jstring>& instance_id,
    const JavaParamRef<jobject>& /* BluetoothGattCharacteristicWrapper */
        bluetooth_gatt_characteristic_wrapper,
    const JavaParamRef<jobject>& /* ChromeBluetoothDevice */
        chrome_bluetooth_device) {
  std::string instance_id_string =
      base::android::ConvertJavaStringToUTF8(env, instance_id);

  DCHECK(!base::Contains(characteristics_, instance_id_string));
  AddCharacteristic(BluetoothRemoteGattCharacteristicAndroid::Create(
      adapter_, this, instance_id_string, bluetooth_gatt_characteristic_wrapper,
      chrome_bluetooth_device));
}

BluetoothRemoteGattServiceAndroid::BluetoothRemoteGattServiceAndroid(
    BluetoothAdapterAndroid* adapter,
    BluetoothDeviceAndroid* device,
    const std::string& instance_id)
    : adapter_(adapter), device_(device), instance_id_(instance_id) {}

void BluetoothRemoteGattServiceAndroid::EnsureCharacteristicsCreated() const {
  if (!characteristics_.empty())
    return;

  // Java call
  Java_ChromeBluetoothRemoteGattService_createCharacteristics(
      AttachCurrentThread(), j_service_);
}

}  // namespace device
