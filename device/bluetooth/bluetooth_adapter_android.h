// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_ANDROID_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_ANDROID_H_

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"

using base::android::ScopedJavaLocalRef;

namespace device {

class BluetoothSocketThread;
class BluetoothDeviceAndroid;

// BluetoothAdapterAndroid, along with the Java class
// org.chromium.device.bluetooth.BluetoothAdapter, implement BluetoothAdapter.
//
// The GATT Profile over Low Energy is supported. LE GATT support has been
// initially built out to support Web Bluetooth, which does not need other
// Bluetooth features. GATT Profile over paired Classic Bluetooth devices may
// work, but it isn't well supported at this time. There is no technical reason
// they can not be well supported should a need arise.
//
// Paired Classic Bluetooth devices visible in the device list for RFCOMM over
// Web Serial.
//
// BluetoothAdapterAndroid is reference counted, and owns the lifetime of the
// Java class BluetoothAdapter via j_adapter_. The adapter also owns a tree of
// additional C++ objects (Devices, Services, Characteristics, Descriptors),
// with each C++ object owning its associated Java class.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdapterAndroid final
    : public BluetoothAdapter {
 public:
  // Create a BluetoothAdapterAndroid instance.
  //
  // |java_bluetooth_adapter_wrapper| is optional. If it is NULL the adapter
  // will return false for |IsPresent()| and not be functional.
  //
  // The BluetoothAdapterAndroid instance will indirectly hold a Java reference
  // to |bluetooth_adapter_wrapper|.
  static scoped_refptr<BluetoothAdapterAndroid> Create(
      const base::android::JavaRef<jobject>&
          bluetooth_adapter_wrapper);  // Java Type: bluetoothAdapterWrapper

  BluetoothAdapterAndroid(const BluetoothAdapterAndroid&) = delete;
  BluetoothAdapterAndroid& operator=(const BluetoothAdapterAndroid&) = delete;

  // BluetoothAdapter:
  void Initialize(base::OnceClosure callback) override;
  std::string GetAddress() const override;
  std::string GetName() const override;
  void SetName(const std::string& name,
               base::OnceClosure callback,
               ErrorCallback error_callback) override;
  bool IsInitialized() const override;
  bool IsPresent() const override;
  bool IsPowered() const override;
  PermissionStatus GetOsPermissionStatus() const override;
  bool IsDiscoverable() const override;
  void SetDiscoverable(bool discoverable,
                       base::OnceClosure callback,
                       ErrorCallback error_callback) override;
  bool IsDiscovering() const override;
  ConstDeviceList GetDevices() const override;
  UUIDList GetUUIDs() const override;
  void CreateRfcommService(const BluetoothUUID& uuid,
                           const ServiceOptions& options,
                           CreateServiceCallback callback,
                           CreateServiceErrorCallback error_callback) override;
  void CreateL2capService(const BluetoothUUID& uuid,
                          const ServiceOptions& options,
                          CreateServiceCallback callback,
                          CreateServiceErrorCallback error_callback) override;
  void RegisterAdvertisement(
      std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
      CreateAdvertisementCallback callback,
      AdvertisementErrorCallback error_callback) override;
  BluetoothLocalGattService* GetGattService(
      const std::string& identifier) const override;

  // Called when adapter state changes.
  void OnAdapterStateChanged(JNIEnv* env,
                             const bool powered);

  // Handles a scan error event by invalidating all discovery sessions.
  void OnScanFailed(JNIEnv* env);

  // Creates or updates device with advertised UUID information when a device is
  // discovered during a scan.
  void CreateOrUpdateDeviceOnScan(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& address,
      const base::android::JavaParamRef<jobject>&
          bluetooth_device_wrapper,  // Java Type: bluetoothDeviceWrapper
      const base::android::JavaParamRef<jstring>& local_name,
      int32_t rssi,
      const base::android::JavaParamRef<jobjectArray>&
          advertised_uuids,  // Java Type: String[]
      int32_t tx_power,
      const base::android::JavaParamRef<jobjectArray>&
          service_data_keys,  // Java Type: String[]
      const base::android::JavaParamRef<jobjectArray>&
          service_data_values,  // Java Type: byte[]
      const base::android::JavaParamRef<jintArray>&
          manufacturer_data_keys,  // Java Type: int[]
      const base::android::JavaParamRef<jobjectArray>&
          manufacturer_data_values,  // Java Type: byte[]
      int32_t advertisement_flags);

  // Called when a new paired device is found or an existing device becomes
  // paired. It creates a device if it isn't in |devices_|
  void PopulateOrUpdatePairedDevice(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& address,
      const base::android::JavaParamRef<jobject>&
          bluetooth_device_wrapper,  // Java Type: BluetoothDeviceWrapper
      bool from_broadcast_receiver);

  // Called when the Android system notifies us that a device is unpaired.
  void OnDeviceUnpaired(JNIEnv* env,
                        const base::android::JavaParamRef<jstring>& address);

  // Updates the connected state of the device with |address| if it's in the
  // device list for |transport| to |connected|. It creates a device if it's
  // not in |devices_| and connected.
  void UpdateDeviceAclConnectState(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& address,
      const base::android::JavaParamRef<jobject>&
          bluetooth_device_wrapper,  // Java Type: BluetoothDeviceWrapper
      uint8_t transport,
      bool connected);

 protected:
  BluetoothAdapterAndroid();
  ~BluetoothAdapterAndroid() override;

  // BluetoothAdapter:
  base::WeakPtr<BluetoothAdapter> GetWeakPtr() override;
  bool SetPoweredImpl(bool powered) override;
  void StartScanWithFilter(
      std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
      DiscoverySessionResultCallback callback) override;
  void UpdateFilter(std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
                    DiscoverySessionResultCallback callback) override;
  void StopScan(DiscoverySessionResultCallback callback) override;
  void RemovePairingDelegateInternal(
      BluetoothDevice::PairingDelegate* pairing_delegate) override;

  void PurgeTimedOutDevices();

  // Utility function used to create a Java object that represents the filter.
  base::android::ScopedJavaLocalRef<jobject> CreateAndroidFilter(
      const BluetoothDiscoveryFilter* discovery_filter);

  // Java object org.chromium.device.bluetooth.ChromeBluetoothAdapter.
  base::android::ScopedJavaGlobalRef<jobject> j_adapter_;

 private:
  void PopulatePairedDevices() const;
  BluetoothDeviceAndroid* CreateDevice(
      const std::string& device_address,
      const base::android::JavaParamRef<jobject>&
          bluetooth_device_wrapper);  // Java Type: BluetoothDeviceWrapper

  // Update device connection states due to adapter turning off because Android
  // doesn't notify ACL connected state broadcast receivers on adapter turning
  // off.
  void UpdateDeviceConnectStatesOnAdapterOff();

  scoped_refptr<BluetoothSocketThread> socket_thread_;

  FRIEND_TEST_ALL_PREFIXES(BluetoothAdapterAndroidTest, ScanFilterTest);
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapterAndroid> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_ANDROID_H_
