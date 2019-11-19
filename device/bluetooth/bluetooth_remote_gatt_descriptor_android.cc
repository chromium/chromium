// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_descriptor_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_android.h"
#include "device/bluetooth/jni_headers/ChromeBluetoothRemoteGattDescriptor_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;

namespace device {

// static
std::unique_ptr<BluetoothRemoteGattDescriptorAndroid>
BluetoothRemoteGattDescriptorAndroid::Create(
    const std::string& instance_id,
    const JavaRef<jobject>& /* BluetoothGattDescriptorWrapper */
    bluetooth_gatt_descriptor_wrapper,
    const JavaRef<jobject>& /* chromeBluetoothDevice */
    chrome_bluetooth_device) {
  std::unique_ptr<BluetoothRemoteGattDescriptorAndroid> descriptor(
      new BluetoothRemoteGattDescriptorAndroid(instance_id));

  descriptor->j_descriptor_.Reset(
      Java_ChromeBluetoothRemoteGattDescriptor_create(
          AttachCurrentThread(), reinterpret_cast<intptr_t>(descriptor.get()),
          bluetooth_gatt_descriptor_wrapper, chrome_bluetooth_device));

  return descriptor;
}

BluetoothRemoteGattDescriptorAndroid::~BluetoothRemoteGattDescriptorAndroid() {
  Java_ChromeBluetoothRemoteGattDescriptor_onBluetoothRemoteGattDescriptorAndroidDestruction(
      AttachCurrentThread(), j_descriptor_);
}

base::android::ScopedJavaLocalRef<jobject>
BluetoothRemoteGattDescriptorAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(j_descriptor_);
}

std::string BluetoothRemoteGattDescriptorAndroid::GetIdentifier() const {
  return instance_id_;
}

BluetoothUUID BluetoothRemoteGattDescriptorAndroid::GetUUID() const {
  return device::BluetoothUUID(
      ConvertJavaStringToUTF8(Java_ChromeBluetoothRemoteGattDescriptor_getUUID(
          AttachCurrentThread(), j_descriptor_)));
}

const std::vector<uint8_t>& BluetoothRemoteGattDescriptorAndroid::GetValue()
    const {
  return value_;
}

BluetoothRemoteGattCharacteristic*
BluetoothRemoteGattDescriptorAndroid::GetCharacteristic() const {
  NOTIMPLEMENTED();
  return nullptr;
}

BluetoothRemoteGattCharacteristic::Permissions
BluetoothRemoteGattDescriptorAndroid::GetPermissions() const {
  NOTIMPLEMENTED();
  return 0;
}

void BluetoothRemoteGattDescriptorAndroid::ReadRemoteDescriptor(
    ValueCallback callback,
    ErrorCallback error_callback) {
  if (read_pending_ || write_pending_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS));
    return;
  }

  if (!Java_ChromeBluetoothRemoteGattDescriptor_readRemoteDescriptor(
          AttachCurrentThread(), j_descriptor_)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       BluetoothRemoteGattServiceAndroid::GATT_ERROR_FAILED));
    return;
  }

  read_pending_ = true;
  read_callback_ = std::move(callback);
  read_error_callback_ = std::move(error_callback);
}

void BluetoothRemoteGattDescriptorAndroid::WriteRemoteDescriptor(
    const std::vector<uint8_t>& new_value,
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
  if (!Java_ChromeBluetoothRemoteGattDescriptor_writeRemoteDescriptor(
          env, j_descriptor_, base::android::ToJavaByteArray(env, new_value))) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       BluetoothRemoteGattServiceAndroid::GATT_ERROR_FAILED));
    return;
  }

  write_pending_ = true;
  write_callback_ = std::move(callback);
  write_error_callback_ = std::move(error_callback);
}

void BluetoothRemoteGattDescriptorAndroid::OnRead(
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
    // TODO(https://crbug.com/584369): Call GattDescriptorValueChanged.
  } else if (!read_error_callback.is_null()) {
    std::move(read_error_callback)
        .Run(BluetoothRemoteGattServiceAndroid::GetGattErrorCode(status));
  }
}

void BluetoothRemoteGattDescriptorAndroid::OnWrite(
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
    // TODO(https://crbug.com/584369): Call GattDescriptorValueChanged.
  } else if (!write_error_callback.is_null()) {
    std::move(write_error_callback)
        .Run(BluetoothRemoteGattServiceAndroid::GetGattErrorCode(status));
  }
}

BluetoothRemoteGattDescriptorAndroid::BluetoothRemoteGattDescriptorAndroid(
    const std::string& instance_id)
    : instance_id_(instance_id) {}

}  // namespace device
