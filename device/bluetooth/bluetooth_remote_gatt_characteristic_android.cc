// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_characteristic_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "device/bluetooth/bluetooth_adapter_android.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor_android.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "device/bluetooth/jni_headers/ChromeBluetoothRemoteGattCharacteristic_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::JavaRef;

namespace device {

// static
std::unique_ptr<BluetoothRemoteGattCharacteristicAndroid>
BluetoothRemoteGattCharacteristicAndroid::Create(
    BluetoothAdapterAndroid* adapter,
    BluetoothRemoteGattServiceAndroid* service,
    const std::string& instance_id,
    const JavaRef<jobject>& /* BluetoothGattCharacteristicWrapper */
    bluetooth_gatt_characteristic_wrapper,
    const JavaRef<
        jobject>& /* ChromeBluetoothDevice */ chrome_bluetooth_device) {
  std::unique_ptr<BluetoothRemoteGattCharacteristicAndroid> characteristic(
      new BluetoothRemoteGattCharacteristicAndroid(adapter, service,
                                                   instance_id));

  JNIEnv* env = AttachCurrentThread();
  characteristic->j_characteristic_.Reset(
      Java_ChromeBluetoothRemoteGattCharacteristic_create(
          env, reinterpret_cast<intptr_t>(characteristic.get()),
          bluetooth_gatt_characteristic_wrapper,
          base::android::ConvertUTF8ToJavaString(env, instance_id),
          chrome_bluetooth_device));

  return characteristic;
}

BluetoothRemoteGattCharacteristicAndroid::
    ~BluetoothRemoteGattCharacteristicAndroid() {
  Java_ChromeBluetoothRemoteGattCharacteristic_onBluetoothRemoteGattCharacteristicAndroidDestruction(
      AttachCurrentThread(), j_characteristic_);
  if (read_callback_) {
    std::move(read_callback_)
        .Run(BluetoothGattService::GattErrorCode::kFailed,
             /*value=*/std::vector<uint8_t>());
  }

  if (!write_callback_.is_null()) {
    DCHECK(!write_error_callback_.is_null());
    std::move(write_error_callback_)
        .Run(BluetoothGattService::GattErrorCode::kFailed);
  }
}

base::android::ScopedJavaLocalRef<jobject>
BluetoothRemoteGattCharacteristicAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(j_characteristic_);
}

std::string BluetoothRemoteGattCharacteristicAndroid::GetIdentifier() const {
  return instance_id_;
}

BluetoothUUID BluetoothRemoteGattCharacteristicAndroid::GetUUID() const {
  return device::BluetoothUUID(ConvertJavaStringToUTF8(
      Java_ChromeBluetoothRemoteGattCharacteristic_getUUID(
          AttachCurrentThread(), j_characteristic_)));
}

const std::vector<uint8_t>& BluetoothRemoteGattCharacteristicAndroid::GetValue()
    const {
  return value_;
}

BluetoothRemoteGattService*
BluetoothRemoteGattCharacteristicAndroid::GetService() const {
  return service_;
}

BluetoothRemoteGattCharacteristic::Properties
BluetoothRemoteGattCharacteristicAndroid::GetProperties() const {
  return Java_ChromeBluetoothRemoteGattCharacteristic_getProperties(
      AttachCurrentThread(), j_characteristic_);
}

BluetoothRemoteGattCharacteristic::Permissions
BluetoothRemoteGattCharacteristicAndroid::GetPermissions() const {
  NOTIMPLEMENTED();
  return 0;
}

std::vector<BluetoothRemoteGattDescriptor*>
BluetoothRemoteGattCharacteristicAndroid::GetDescriptors() const {
  EnsureDescriptorsCreated();
  return BluetoothRemoteGattCharacteristic::GetDescriptors();
}

BluetoothRemoteGattDescriptor*
BluetoothRemoteGattCharacteristicAndroid::GetDescriptor(
    const std::string& identifier) const {
  EnsureDescriptorsCreated();
  return BluetoothRemoteGattCharacteristic::GetDescriptor(identifier);
}

std::vector<BluetoothRemoteGattDescriptor*>
BluetoothRemoteGattCharacteristicAndroid::GetDescriptorsByUUID(
    const BluetoothUUID& uuid) const {
  EnsureDescriptorsCreated();
  return BluetoothRemoteGattCharacteristic::GetDescriptorsByUUID(uuid);
}

void BluetoothRemoteGattCharacteristicAndroid::ReadRemoteCharacteristic(
    ValueCallback callback) {
  if (read_pending_ || write_pending_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       BluetoothGattService::GattErrorCode::kInProgress,
                       /*value=*/std::vector<uint8_t>()));
    return;
  }

  if (!Java_ChromeBluetoothRemoteGattCharacteristic_readRemoteCharacteristic(
          AttachCurrentThread(), j_characteristic_)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  BluetoothGattService::GattErrorCode::kFailed,
                                  /*value=*/std::vector<uint8_t>()));
    return;
  }

  read_pending_ = true;
  read_callback_ = std::move(callback);
}

void BluetoothRemoteGattCharacteristicAndroid::WriteRemoteCharacteristic(
    const std::vector<uint8_t>& value,
    WriteType write_type,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  if (read_pending_ || write_pending_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       BluetoothGattService::GattErrorCode::kInProgress));
    return;
  }

  AndroidWriteType android_write_type;
  switch (write_type) {
    case WriteType::kWithResponse:
      android_write_type = AndroidWriteType::kDefault;
      break;
    case WriteType::kWithoutResponse:
      android_write_type = AndroidWriteType::kNoResponse;
      break;
  }

  JNIEnv* env = AttachCurrentThread();
  if (!Java_ChromeBluetoothRemoteGattCharacteristic_writeRemoteCharacteristic(
          env, j_characteristic_, base::android::ToJavaByteArray(env, value),
          static_cast<int>(android_write_type))) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       BluetoothGattService::GattErrorCode::kFailed));
    return;
  }

  write_pending_ = true;
  write_callback_ = std::move(callback);
  write_error_callback_ = std::move(error_callback);
}

void BluetoothRemoteGattCharacteristicAndroid::
    DeprecatedWriteRemoteCharacteristic(const std::vector<uint8_t>& value,
                                        base::OnceClosure callback,
                                        ErrorCallback error_callback) {
  if (read_pending_ || write_pending_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       BluetoothGattService::GattErrorCode::kInProgress));
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  if (!Java_ChromeBluetoothRemoteGattCharacteristic_writeRemoteCharacteristic(
          env, j_characteristic_, base::android::ToJavaByteArray(env, value),
          static_cast<int>(AndroidWriteType::kNone))) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       BluetoothGattService::GattErrorCode::kFailed));
    return;
  }

  write_pending_ = true;
  write_callback_ = std::move(callback);
  write_error_callback_ = std::move(error_callback);
}

void BluetoothRemoteGattCharacteristicAndroid::OnChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jbyteArray>& value) {
  base::android::JavaByteArrayToByteVector(env, value, &value_);
  adapter_->NotifyGattCharacteristicValueChanged(this, value_);
}

void BluetoothRemoteGattCharacteristicAndroid::OnRead(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    int32_t status,
    const JavaParamRef<jbyteArray>& value) {
  read_pending_ = false;

  // Clear callbacks before calling to avoid reentrancy issues.
  ValueCallback read_callback = std::move(read_callback_);
  if (!read_callback)
    return;

  if (status == 0) {  // android.bluetooth.BluetoothGatt.GATT_SUCCESS
    base::android::JavaByteArrayToByteVector(env, value, &value_);
    std::move(read_callback).Run(/*error_code=*/std::nullopt, value_);
  } else {
    std::move(read_callback)
        .Run(BluetoothRemoteGattServiceAndroid::GetGattErrorCode(status),
             /*value=*/std::vector<uint8_t>());
  }
}

void BluetoothRemoteGattCharacteristicAndroid::OnWrite(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    int32_t status) {
  write_pending_ = false;

  // Clear callbacks before calling to avoid reentrancy issues.
  base::OnceClosure write_callback = std::move(write_callback_);
  ErrorCallback write_error_callback = std::move(write_error_callback_);

  if (status == 0  // android.bluetooth.BluetoothGatt.GATT_SUCCESS
      && !write_callback.is_null()) {
    std::move(write_callback).Run();
  } else if (!write_error_callback.is_null()) {
    std::move(write_error_callback)
        .Run(BluetoothRemoteGattServiceAndroid::GetGattErrorCode(status));
  }
}

void BluetoothRemoteGattCharacteristicAndroid::CreateGattRemoteDescriptor(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller,
    const JavaParamRef<jstring>& instanceId,
    const JavaParamRef<jobject>& /* BluetoothGattDescriptorWrapper */
    bluetooth_gatt_descriptor_wrapper,
    const JavaParamRef<jobject>& /* ChromeBluetoothDevice */
    chrome_bluetooth_device) {
  std::string instanceIdString = ConvertJavaStringToUTF8(env, instanceId);

  DCHECK(!base::Contains(descriptors_, instanceIdString));
  AddDescriptor(BluetoothRemoteGattDescriptorAndroid::Create(
      instanceIdString, bluetooth_gatt_descriptor_wrapper,
      chrome_bluetooth_device));
}

void BluetoothRemoteGattCharacteristicAndroid::SubscribeToNotifications(
    BluetoothRemoteGattDescriptor* ccc_descriptor,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  if (!Java_ChromeBluetoothRemoteGattCharacteristic_setCharacteristicNotification(
          AttachCurrentThread(), j_characteristic_, true)) {
    LOG(ERROR) << "Error enabling characteristic notification";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       BluetoothGattService::GattErrorCode::kFailed));
    return;
  }

  bool hasNotify = GetProperties() & PROPERTY_NOTIFY;
  std::vector<uint8_t> value(2);
  value[0] = hasNotify ? 1 : 2;

  ccc_descriptor->WriteRemoteDescriptor(value, std::move(callback),
                                        std::move(error_callback));
}

void BluetoothRemoteGattCharacteristicAndroid::UnsubscribeFromNotifications(
    BluetoothRemoteGattDescriptor* ccc_descriptor,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  if (!Java_ChromeBluetoothRemoteGattCharacteristic_setCharacteristicNotification(
          AttachCurrentThread(), j_characteristic_, false)) {
    LOG(ERROR) << "Error disabling characteristic notification";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       device::BluetoothGattService::GattErrorCode::kFailed));
    return;
  }

  std::vector<uint8_t> value(2);
  value[0] = 0;

  ccc_descriptor->WriteRemoteDescriptor(value, std::move(callback),
                                        std::move(error_callback));
}

BluetoothRemoteGattCharacteristicAndroid::
    BluetoothRemoteGattCharacteristicAndroid(
        BluetoothAdapterAndroid* adapter,
        BluetoothRemoteGattServiceAndroid* service,
        const std::string& instance_id)
    : adapter_(adapter), service_(service), instance_id_(instance_id) {}

void BluetoothRemoteGattCharacteristicAndroid::EnsureDescriptorsCreated()
    const {
  if (!descriptors_.empty())
    return;

  Java_ChromeBluetoothRemoteGattCharacteristic_createDescriptors(
      AttachCurrentThread(), j_characteristic_);
}

}  // namespace device
