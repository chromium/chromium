// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_ANDROID_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_ANDROID_H_

#include <stdint.h>

#include <memory>
#include <optional>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter_android.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"

namespace device {

class BluetoothSocketThread;
class BluetoothUUID;

// BluetoothDeviceAndroid along with its owned Java class
// org.chromium.device.bluetooth.ChromeBluetoothDevice implement
// BluetoothDevice.
class DEVICE_BLUETOOTH_EXPORT BluetoothDeviceAndroid final
    : public BluetoothDevice {
 public:
  // Create a BluetoothDeviceAndroid instance and associated Java
  // ChromeBluetoothDevice using the provided |java_bluetooth_device_wrapper|.
  //
  // The ChromeBluetoothDevice instance will hold a Java reference
  // to |bluetooth_device_wrapper|.
  static std::unique_ptr<BluetoothDeviceAndroid> Create(
      BluetoothAdapterAndroid* adapter,
      const base::android::JavaRef<jobject>&
          bluetooth_device_wrapper,  // Java Type: BluetoothDeviceWrapper
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<BluetoothSocketThread> socket_thread);

  BluetoothDeviceAndroid(const BluetoothDeviceAndroid&) = delete;
  BluetoothDeviceAndroid& operator=(const BluetoothDeviceAndroid&) = delete;

  ~BluetoothDeviceAndroid() override;

  // Returns the associated ChromeBluetoothDevice Java object.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Get owning BluetoothAdapter cast to BluetoothAdapterAndroid.
  BluetoothAdapterAndroid* GetAndroidAdapter() {
    return static_cast<BluetoothAdapterAndroid*>(adapter_);
  }

  // BluetoothDevice:
  uint32_t GetBluetoothClass() const override;
  BluetoothTransport GetType() const override;
  std::string GetAddress() const override;
  AddressType GetAddressType() const override;
  VendorIDSource GetVendorIDSource() const override;
  uint16_t GetVendorID() const override;
  uint16_t GetProductID() const override;
  uint16_t GetDeviceID() const override;
  uint16_t GetAppearance() const override;
  std::optional<std::string> GetName() const override;
  bool IsPaired() const override;
  bool IsConnected() const override;
  bool IsGattConnected() const override;
  bool IsConnectable() const override;
  bool IsConnecting() const override;
  UUIDSet GetUUIDs() const override;
  bool ExpectingPinCode() const override;
  bool ExpectingPasskey() const override;
  bool ExpectingConfirmation() const override;
  void GetConnectionInfo(ConnectionInfoCallback callback) override;
  void SetConnectionLatency(ConnectionLatency connection_latency,
                            base::OnceClosure callback,
                            ErrorCallback error_callback) override;
  void Connect(PairingDelegate* pairing_delegate,
               ConnectCallback callback) override;
  void SetPinCode(const std::string& pincode) override;
  void SetPasskey(uint32_t passkey) override;
  void ConfirmPairing() override;
  void RejectPairing() override;
  void CancelPairing() override;
  void Disconnect(base::OnceClosure callback,
                  ErrorCallback error_callback) override;
  void Forget(base::OnceClosure callback,
              ErrorCallback error_callback) override;
  void ConnectToService(const device::BluetoothUUID& uuid,
                        ConnectToServiceCallback callback,
                        ConnectToServiceErrorCallback error_callback) override;
  void ConnectToServiceInsecurely(
      const device::BluetoothUUID& uuid,
      ConnectToServiceCallback callback,
      ConnectToServiceErrorCallback error_callback) override;

  // Callback indicating when GATT client has connected/disconnected.
  // See android.bluetooth.BluetoothGattCallback.onConnectionStateChange.
  void OnConnectionStateChange(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      int32_t status,
      bool connected);

  // Callback indicating when all services of the device have been
  // discovered.
  void OnGattServicesDiscovered(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

  // Creates Bluetooth GATT service objects and adds them to
  // BluetoothDevice::gatt_services_ if they are not already there.
  void CreateGattRemoteService(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      const base::android::JavaParamRef<jstring>& instance_id,
      const base::android::JavaParamRef<jobject>&
          bluetooth_gatt_service_wrapper);  // BluetoothGattServiceWrapper

  // Update the connected state of |transport| to |connected|.
  void UpdateAclConnectState(uint8_t transport, bool connected);
  bool is_acl_connected() { return connected_transport_; }

 private:
  BluetoothDeviceAndroid(
      BluetoothAdapterAndroid* adapter,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<BluetoothSocketThread> socket_thread);

  // BluetoothDevice:
  void CreateGattConnectionImpl(
      std::optional<device::BluetoothUUID> service_uuid) override;
  void DisconnectGatt() override;

  void LoadInitialCachedMetadata();

  // Java object org.chromium.device.bluetooth.ChromeBluetoothDevice.
  base::android::ScopedJavaGlobalRef<jobject> j_device_;

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<BluetoothSocketThread> socket_thread_;

  bool gatt_connected_ = false;

  // A bit-wise flag indicating connected states of Bluetooth transports.
  uint8_t connected_transport_ = 0;

  // Cached values to serve when the Bluetooth adapter is off and the Android
  // system doesn't serve them.
  mutable std::optional<std::string> cached_name_;
  mutable uint32_t cached_class_;
  mutable BluetoothTransport cached_type_;
  mutable bool cached_paired_;
  mutable UUIDSet cached_sdp_uuids_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_ANDROID_H_
