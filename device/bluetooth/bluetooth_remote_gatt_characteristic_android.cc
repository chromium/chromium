// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_characteristic_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/bluetooth/bluetooth_adapter_android.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor_android.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_android.h"
#include "device/bluetooth/jni_headers/ChromeBluetoothRemoteGattCharacteristic_jni.h"

using base::android::AttachCurrentThread;
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
  if (!read_callback_.is_null()) {
    DCHECK(!read_error_callback_.is_null());
    std::move(read_error_callback_)
        .Run(BluetoothGattService::GATT_ERROR_FAILED);
  }

  if (!write_callback_.is_null()) {
    DCHECK(!write_error_callback_.is_null());
    std::move(write_error_callback_)
        .Run(BluetoothGattService::GATT_ERROR_FAILED);
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
    ValueCallback callback,
    ErrorCallback error_callback) {
  if (read_pending_ || write_pending_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS));
    return;
  }

  if (!Java_ChromeBluetoothRemoteGattCharacteristic_readRemoteCharacteristic(
          AttachCurrentThread(), j_characteristic_)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  read_pending_ = true;
  read_callback_ = std::move(callback);
  read_error_callback_ = std::move(error_callback);
}

void BluetoothRemoteGattCharacteristicAndroid::WriteRemoteCharacteristic(
    const std::vector<uint8_t>& value,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  if (read_pending_ || write_pending_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS));
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  if (!Java_ChromeBluetoothRemoteGattCharacteristic_writeRemoteCharacteristic(
          env, j_characteristic_, base::android::ToJavaByteArray(env, value))) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       BluetoothRemoteGattService::GATT_ERROR_FAILED));
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
  ErrorCallback read_error_callback = std::move(read_error_callback_);

  if (status == 0  // android.bluetooth.BluetoothGatt.GATT_SUCCESS
      && !read_callback.is_null()) {
    base::android::JavaByteArrayToByteVector(env, value, &value_);
    std::move(read_callback).Run(value_);
  } else if (!read_error_callback.is_null()) {
    std::move(read_error_callback)
        .Run(BluetoothRemoteGattServiceAndroid::GetGattErrorCode(status));
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
  std::string instanceIdString =
      base::android::ConvertJavaStringToUTF8(env, instanceId);

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
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       BluetoothRemoteGattService::GATT_ERROR_FAILED));
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
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       device::BluetoothRemoteGattService::GATT_ERROR_FAILED));
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
