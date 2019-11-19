// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_ANDROID_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_ANDROID_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"

namespace device {

// BluetoothRemoteGattDescriptorAndroid along with its owned Java class
// org.chromium.device.bluetooth.ChromeBluetoothRemoteGattDescriptor
// implement BluetootGattDescriptor.
class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattDescriptorAndroid
    : public BluetoothRemoteGattDescriptor {
 public:
  // Create a BluetoothRemoteGattDescriptorAndroid instance and associated
  // Java ChromeBluetoothRemoteGattDescriptor using the provided
  // |bluetooth_gatt_descriptor_wrapper|.
  //
  // The ChromeBluetoothRemoteGattDescriptor instance will hold a Java
  // reference to |bluetooth_gatt_descriptor_wrapper|.
  static std::unique_ptr<BluetoothRemoteGattDescriptorAndroid> Create(
      const std::string& instanceId,
      const base::android::JavaRef<jobject>& /* BluetoothGattDescriptorWrapper
                                                */
      bluetooth_gatt_descriptor_wrapper,
      const base::android::JavaRef<
          jobject>& /* chromeBluetoothDevice */ chrome_bluetooth_device);

  ~BluetoothRemoteGattDescriptorAndroid() override;

  // Returns the associated ChromeBluetoothRemoteGattDescriptor Java object.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // BluetoothRemoteGattDescriptor interface:
  std::string GetIdentifier() const override;
  BluetoothUUID GetUUID() const override;
  const std::vector<uint8_t>& GetValue() const override;
  BluetoothRemoteGattCharacteristic* GetCharacteristic() const override;
  BluetoothRemoteGattCharacteristic::Permissions GetPermissions()
      const override;
  void ReadRemoteDescriptor(ValueCallback callback,
                            ErrorCallback error_callback) override;
  void WriteRemoteDescriptor(const std::vector<uint8_t>& value,
                             base::OnceClosure callback,
                             ErrorCallback error_callback) override;

  // Called when Read operation completes.
  void OnRead(JNIEnv* env,
              const base::android::JavaParamRef<jobject>& jcaller,
              int32_t status,
              const base::android::JavaParamRef<jbyteArray>& value);

  // Called when Write operation completes.
  void OnWrite(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& jcaller,
               int32_t status);

 private:
  BluetoothRemoteGattDescriptorAndroid(const std::string& instanceId);

  // Java object
  // org.chromium.device.bluetooth.ChromeBluetoothRemoteGattDescriptor.
  base::android::ScopedJavaGlobalRef<jobject> j_descriptor_;

  // Adapter unique instance ID.
  std::string instance_id_;

  // ReadRemoteCharacteristic callbacks and pending state.
  bool read_pending_ = false;
  ValueCallback read_callback_;
  ErrorCallback read_error_callback_;

  // WriteRemoteCharacteristic callbacks and pending state.
  bool write_pending_ = false;
  base::OnceClosure write_callback_;
  ErrorCallback write_error_callback_;

  std::vector<uint8_t> value_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattDescriptorAndroid);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_ANDROID_H_
