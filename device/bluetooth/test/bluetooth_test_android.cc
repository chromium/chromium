// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/bluetooth_test_android.h"

#include <algorithm>
#include <iterator>
#include <sstream>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "device/base/features.h"
#include "device/bluetooth/android/wrappers.h"
#include "device/bluetooth/bluetooth_adapter_android.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_device_android.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic_android.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor_android.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_android.h"
#include "device/bluetooth/bluetooth_socket_android.h"
#include "device/bluetooth/test/bluetooth_test.h"
#include "device/bluetooth/test/test_bluetooth_adapter_observer.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "device/bluetooth_test_jni_headers/Fakes_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace device {

BluetoothTestAndroid::BluetoothTestAndroid() = default;

BluetoothTestAndroid::BluetoothTestAndroid(
    base::test::TaskEnvironment::TimeSource time_source)
    : BluetoothTestBase(time_source) {}

BluetoothTestAndroid::~BluetoothTestAndroid() {
}

void BluetoothTestAndroid::SetUp() {
  // Set the permission to true so that we can use the API.
  Java_Fakes_setLocationServicesState(AttachCurrentThread(),
                                      true /* isEnabled */);
  Java_Fakes_initFakeThreadUtilsWrapper(AttachCurrentThread(),
                                        reinterpret_cast<intptr_t>(this));
}

void BluetoothTestAndroid::TearDown() {
  // Unit tests are able to reset the adapter themselves (e.g.
  // BluetoothTest::TogglePowerFakeAdapter_DestroyWithPending), so this check is
  // necessary.
  if (adapter_) {
    BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
    for (auto* device : devices) {
      DeleteDevice(device);
    }
  }
  EXPECT_EQ(0, gatt_open_connections_);

  BluetoothTestBase::TearDown();
}

static void RunJavaRunnable(
    const base::android::ScopedJavaGlobalRef<jobject>& runnable_ref) {
  Java_Fakes_runRunnable(AttachCurrentThread(), runnable_ref);
}

void BluetoothTestAndroid::PostTaskFromJava(
    JNIEnv* env,
    const JavaParamRef<jobject>& runnable) {
  base::android::ScopedJavaGlobalRef<jobject> runnable_ref;
  // ScopedJavaGlobalRef does not hold onto the env reference, so it is safe to
  // use it across threads. |RunJavaRunnable| will acquire a new JNIEnv before
  // running the Runnable.
  runnable_ref.Reset(env, runnable);
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&RunJavaRunnable, runnable_ref));
}

void BluetoothTestAndroid::PostDelayedTaskFromJava(
    JNIEnv* env,
    const JavaParamRef<jobject>& runnable,
    jlong delayMillis) {
  base::android::ScopedJavaGlobalRef<jobject> runnable_ref;
  // ScopedJavaGlobalRef does not hold onto the env reference, so it is safe to
  // use it across threads. |RunJavaRunnable| will acquire a new JNIEnv before
  // running the Runnable.
  runnable_ref.Reset(env, runnable);
  task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&RunJavaRunnable, runnable_ref),
      base::Milliseconds(delayMillis));
}

bool BluetoothTestAndroid::PlatformSupportsLowEnergy() {
  return true;
}

void BluetoothTestAndroid::InitWithDefaultAdapter() {
  j_default_bluetooth_adapter_ =
      BluetoothAdapterWrapper_CreateWithDefaultAdapter(
          base::FeatureList::IsEnabled(features::kBluetoothRfcommAndroid));
  adapter_ = BluetoothAdapterAndroid::Create(j_default_bluetooth_adapter_);
}

void BluetoothTestAndroid::InitWithoutDefaultAdapter() {
  adapter_ = BluetoothAdapterAndroid::Create(nullptr);
}

void BluetoothTestAndroid::InitWithFakeAdapter() {
  j_fake_bluetooth_adapter_.Reset(Java_FakeBluetoothAdapter_create(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this)));

  adapter_ = BluetoothAdapterAndroid::Create(j_fake_bluetooth_adapter_).get();
}

bool BluetoothTestAndroid::DenyPermission() {
  Java_FakeBluetoothAdapter_setFakePermission(AttachCurrentThread(),
                                              j_fake_bluetooth_adapter_, false);
  return true;
}

BluetoothDevice* BluetoothTestAndroid::SimulateLowEnergyDevice(
    int device_ordinal) {
  TestBluetoothAdapterObserver observer(adapter_);
  Java_FakeBluetoothAdapter_simulateLowEnergyDevice(
      AttachCurrentThread(), j_fake_bluetooth_adapter_, device_ordinal);
  return observer.last_device();
}

BluetoothDevice* BluetoothTestAndroid::SimulateClassicDevice() {
  std::string address = SimulatePairedClassicDevice(0);
  return adapter_->GetDevice(address);
}

std::string BluetoothTestAndroid::SimulatePairedClassicDevice(
    int device_ordinal,
    bool notify_callback) {
  std::string address = Java_FakeBluetoothAdapter_simulatePairedClassicDevice(
      AttachCurrentThread(), j_fake_bluetooth_adapter_, device_ordinal,
      notify_callback);
  // BluetoothAdapterAndroid only pulls bonded devices from the system when
  // GetDevices() is called.
  adapter_->GetDevices();
  return address;
}

void BluetoothTestAndroid::UnpairDevice(std::string address) {
  Java_FakeBluetoothAdapter_unpairDevice(AttachCurrentThread(),
                                         j_fake_bluetooth_adapter_, address);
}

void BluetoothTestAndroid::SimulateAclConnectStateChange(
    BluetoothDevice* device,
    uint8_t transport,
    bool connected) {
  BluetoothDeviceAndroid* device_android =
      static_cast<BluetoothDeviceAndroid*>(device);
  Java_FakeBluetoothDevice_aclConnectionStateChange(
      AttachCurrentThread(), device_android->GetJavaObject(), transport,
      connected);
}

void BluetoothTestAndroid::RememberDeviceForSubsequentAction(
    BluetoothDevice* device) {
  BluetoothDeviceAndroid* device_android =
      static_cast<BluetoothDeviceAndroid*>(device);

  Java_FakeBluetoothDevice_rememberDeviceForSubsequentAction(
      base::android::AttachCurrentThread(), device_android->GetJavaObject());
}

void BluetoothTestAndroid::SetEnabledDeviceTransport(
    BluetoothTransport transport) {
  Java_FakeBluetoothAdapter_setEnabledTransport(
      AttachCurrentThread(), j_fake_bluetooth_adapter_, transport);
}

void BluetoothTestAndroid::SimulateLocationServicesOff() {
  Java_Fakes_setLocationServicesState(AttachCurrentThread(),
                                      false /* isEnabled */);
}

void BluetoothTestAndroid::ForceIllegalStateException() {
  Java_FakeBluetoothAdapter_forceIllegalStateException(
      AttachCurrentThread(), j_fake_bluetooth_adapter_);
}

void BluetoothTestAndroid::FailCurrentLeScan(int error_code) {
  Java_FakeBluetoothAdapter_failCurrentLeScan(
      AttachCurrentThread(), j_fake_bluetooth_adapter_, error_code);
}

void BluetoothTestAndroid::SimulateGattConnection(BluetoothDevice* device) {
  BluetoothDeviceAndroid* device_android =
      static_cast<BluetoothDeviceAndroid*>(device);

  Java_FakeBluetoothDevice_connectionStateChange(
      AttachCurrentThread(), device_android->GetJavaObject(),
      0,      // android.bluetooth.BluetoothGatt.GATT_SUCCESS
      true);  // connected
}

void BluetoothTestAndroid::SimulateGattConnectionError(
    BluetoothDevice* device,
    BluetoothDevice::ConnectErrorCode) {
  BluetoothDeviceAndroid* device_android =
      static_cast<BluetoothDeviceAndroid*>(device);

  Java_FakeBluetoothDevice_connectionStateChange(
      AttachCurrentThread(), device_android->GetJavaObject(),
      // TODO(ortuno): Add all types of errors Android can produce. For now we
      // just return a timeout error.
      // http://crbug.com/578191
      0x08,    // Connection Timeout from Bluetooth Spec.
      false);  // connected
}

void BluetoothTestAndroid::SimulateGattDisconnection(BluetoothDevice* device) {
  BluetoothDeviceAndroid* device_android =
      static_cast<BluetoothDeviceAndroid*>(device);

  Java_FakeBluetoothDevice_connectionStateChange(
      AttachCurrentThread(), device_android->GetJavaObject(),
      0x13,    // Connection terminate by peer user from Bluetooth Spec.
      false);  // disconnected
}

void BluetoothTestAndroid::SimulateGattServicesDiscovered(
    BluetoothDevice* device,
    const std::vector<std::string>& uuids,
    const std::vector<std::string>& blocked_uuids) {
  DCHECK(blocked_uuids.empty()) << "Setting blocked_uuids unsupported.";
  BluetoothDeviceAndroid* device_android = nullptr;
  if (device) {
    device_android = static_cast<BluetoothDeviceAndroid*>(device);
  }

  // Join UUID strings into a single string.
  std::ostringstream uuids_space_delimited;
  std::ranges::copy(
      uuids, std::ostream_iterator<std::string>(uuids_space_delimited, " "));

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FakeBluetoothDevice_servicesDiscovered(
      env, device_android ? device_android->GetJavaObject() : nullptr,
      0,  // android.bluetooth.BluetoothGatt.GATT_SUCCESS
      base::android::ConvertUTF8ToJavaString(env, uuids_space_delimited.str()));
}

void BluetoothTestAndroid::SimulateGattServicesDiscoveryError(
    BluetoothDevice* device) {
  BluetoothDeviceAndroid* device_android = nullptr;
  if (device) {
    device_android = static_cast<BluetoothDeviceAndroid*>(device);
  }

  Java_FakeBluetoothDevice_servicesDiscovered(
      AttachCurrentThread(),
      device_android ? device_android->GetJavaObject() : nullptr,
      0x00000101,  // android.bluetooth.BluetoothGatt.GATT_FAILURE
      nullptr);
}

void BluetoothTestAndroid::SimulateGattCharacteristic(
    BluetoothRemoteGattService* service,
    const std::string& uuid,
    int properties) {
  BluetoothRemoteGattServiceAndroid* service_android =
      static_cast<BluetoothRemoteGattServiceAndroid*>(service);
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_FakeBluetoothGattService_addCharacteristic(
      env, service_android->GetJavaObject(),
      base::android::ConvertUTF8ToJavaString(env, uuid), properties);
}

void BluetoothTestAndroid::RememberCharacteristicForSubsequentAction(
    BluetoothRemoteGattCharacteristic* characteristic) {
  BluetoothRemoteGattCharacteristicAndroid* characteristic_android =
      static_cast<BluetoothRemoteGattCharacteristicAndroid*>(characteristic);

  Java_FakeBluetoothGattCharacteristic_rememberCharacteristicForSubsequentAction(
      base::android::AttachCurrentThread(),
      characteristic_android->GetJavaObject());
}

void BluetoothTestAndroid::RememberCCCDescriptorForSubsequentAction(
    BluetoothRemoteGattCharacteristic* characteristic) {
  remembered_ccc_descriptor_ =
      characteristic
          ->GetDescriptorsByUUID(BluetoothRemoteGattDescriptor::
                                     ClientCharacteristicConfigurationUuid())
          .at(0);
  DCHECK(remembered_ccc_descriptor_);
  RememberDescriptorForSubsequentAction(remembered_ccc_descriptor_);
}

void BluetoothTestAndroid::SimulateGattNotifySessionStarted(
    BluetoothRemoteGattCharacteristic* characteristic) {
  BluetoothRemoteGattDescriptorAndroid* descriptor_android = nullptr;
  if (characteristic) {
    descriptor_android = static_cast<BluetoothRemoteGattDescriptorAndroid*>(
        characteristic
            ->GetDescriptorsByUUID(BluetoothRemoteGattDescriptor::
                                       ClientCharacteristicConfigurationUuid())
            .at(0));
  }
  Java_FakeBluetoothGattDescriptor_valueWrite(
      base::android::AttachCurrentThread(),
      descriptor_android ? descriptor_android->GetJavaObject() : nullptr,
      0);  // android.bluetooth.BluetoothGatt.GATT_SUCCESS
}

void BluetoothTestAndroid::SimulateGattNotifySessionStartError(
    BluetoothRemoteGattCharacteristic* characteristic,
    BluetoothGattService::GattErrorCode error_code) {
  BluetoothRemoteGattDescriptorAndroid* descriptor_android = nullptr;
  if (characteristic) {
    descriptor_android = static_cast<BluetoothRemoteGattDescriptorAndroid*>(
        characteristic
            ->GetDescriptorsByUUID(BluetoothRemoteGattDescriptor::
                                       ClientCharacteristicConfigurationUuid())
            .at(0));
  }
  Java_FakeBluetoothGattDescriptor_valueWrite(
      base::android::AttachCurrentThread(),
      descriptor_android ? descriptor_android->GetJavaObject() : nullptr,
      BluetoothRemoteGattServiceAndroid::GetAndroidErrorCode(error_code));
}

void BluetoothTestAndroid::SimulateGattNotifySessionStopped(
    BluetoothRemoteGattCharacteristic* characteristic) {
  BluetoothRemoteGattDescriptorAndroid* descriptor_android = nullptr;
  if (characteristic) {
    descriptor_android = static_cast<BluetoothRemoteGattDescriptorAndroid*>(
        characteristic
            ->GetDescriptorsByUUID(BluetoothRemoteGattDescriptor::
                                       ClientCharacteristicConfigurationUuid())
            .at(0));
  }
  Java_FakeBluetoothGattDescriptor_valueWrite(
      base::android::AttachCurrentThread(),
      descriptor_android ? descriptor_android->GetJavaObject() : nullptr,
      0);  // android.bluetooth.BluetoothGatt.GATT_SUCCESS
}

void BluetoothTestAndroid::SimulateGattNotifySessionStopError(
    BluetoothRemoteGattCharacteristic* characteristic,
    BluetoothGattService::GattErrorCode error_code) {
  BluetoothRemoteGattDescriptorAndroid* descriptor_android = nullptr;
  if (characteristic) {
    descriptor_android = static_cast<BluetoothRemoteGattDescriptorAndroid*>(
        characteristic
            ->GetDescriptorsByUUID(BluetoothRemoteGattDescriptor::
                                       ClientCharacteristicConfigurationUuid())
            .at(0));
  }
  Java_FakeBluetoothGattDescriptor_valueWrite(
      base::android::AttachCurrentThread(),
      descriptor_android ? descriptor_android->GetJavaObject() : nullptr,
      BluetoothRemoteGattServiceAndroid::GetAndroidErrorCode(error_code));
}

void BluetoothTestAndroid::
    SimulateGattCharacteristicSetNotifyWillFailSynchronouslyOnce(
        BluetoothRemoteGattCharacteristic* characteristic) {
  BluetoothRemoteGattCharacteristicAndroid* characteristic_android =
      static_cast<BluetoothRemoteGattCharacteristicAndroid*>(characteristic);
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_FakeBluetoothGattCharacteristic_setCharacteristicNotificationWillFailSynchronouslyOnce(
      env, characteristic_android->GetJavaObject());
}

void BluetoothTestAndroid::SimulateGattCharacteristicChanged(
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  BluetoothRemoteGattCharacteristicAndroid* characteristic_android =
      static_cast<BluetoothRemoteGattCharacteristicAndroid*>(characteristic);
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_FakeBluetoothGattCharacteristic_valueChanged(
      env, characteristic_android ? characteristic_android->GetJavaObject()
                                  : nullptr,
      base::android::ToJavaByteArray(env, value));
}

void BluetoothTestAndroid::SimulateGattCharacteristicRead(
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  BluetoothRemoteGattCharacteristicAndroid* characteristic_android =
      static_cast<BluetoothRemoteGattCharacteristicAndroid*>(characteristic);
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_FakeBluetoothGattCharacteristic_valueRead(
      env, characteristic_android ? characteristic_android->GetJavaObject()
                                  : nullptr,
      0,  // android.bluetooth.BluetoothGatt.GATT_SUCCESS
      base::android::ToJavaByteArray(env, value));
}

void BluetoothTestAndroid::SimulateGattCharacteristicReadError(
    BluetoothRemoteGattCharacteristic* characteristic,
    BluetoothGattService::GattErrorCode error_code) {
  BluetoothRemoteGattCharacteristicAndroid* characteristic_android =
      static_cast<BluetoothRemoteGattCharacteristicAndroid*>(characteristic);
  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<uint8_t> empty_value;

  Java_FakeBluetoothGattCharacteristic_valueRead(
      env, characteristic_android->GetJavaObject(),
      BluetoothRemoteGattServiceAndroid::GetAndroidErrorCode(error_code),
      base::android::ToJavaByteArray(env, empty_value));
}

void BluetoothTestAndroid::
    SimulateGattCharacteristicReadWillFailSynchronouslyOnce(
        BluetoothRemoteGattCharacteristic* characteristic) {
  BluetoothRemoteGattCharacteristicAndroid* characteristic_android =
      static_cast<BluetoothRemoteGattCharacteristicAndroid*>(characteristic);
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_FakeBluetoothGattCharacteristic_setReadCharacteristicWillFailSynchronouslyOnce(
      env, characteristic_android->GetJavaObject());
}

void BluetoothTestAndroid::SimulateGattCharacteristicWrite(
    BluetoothRemoteGattCharacteristic* characteristic) {
  BluetoothRemoteGattCharacteristicAndroid* characteristic_android =
      static_cast<BluetoothRemoteGattCharacteristicAndroid*>(characteristic);
  Java_FakeBluetoothGattCharacteristic_valueWrite(
      base::android::AttachCurrentThread(),
      characteristic_android ? characteristic_android->GetJavaObject()
                             : nullptr,
      0);  // android.bluetooth.BluetoothGatt.GATT_SUCCESS
}

void BluetoothTestAndroid::SimulateGattCharacteristicWriteError(
    BluetoothRemoteGattCharacteristic* characteristic,
    BluetoothGattService::GattErrorCode error_code) {
  BluetoothRemoteGattCharacteristicAndroid* characteristic_android =
      static_cast<BluetoothRemoteGattCharacteristicAndroid*>(characteristic);
  Java_FakeBluetoothGattCharacteristic_valueWrite(
      base::android::AttachCurrentThread(),
      characteristic_android->GetJavaObject(),
      BluetoothRemoteGattServiceAndroid::GetAndroidErrorCode(error_code));
}

void BluetoothTestAndroid::
    SimulateGattCharacteristicWriteWillFailSynchronouslyOnce(
        BluetoothRemoteGattCharacteristic* characteristic) {
  BluetoothRemoteGattCharacteristicAndroid* characteristic_android =
      static_cast<BluetoothRemoteGattCharacteristicAndroid*>(characteristic);
  Java_FakeBluetoothGattCharacteristic_setWriteCharacteristicWillFailSynchronouslyOnce(
      base::android::AttachCurrentThread(),
      characteristic_android->GetJavaObject());
}

void BluetoothTestAndroid::SimulateGattDescriptor(
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::string& uuid) {
  BluetoothRemoteGattCharacteristicAndroid* characteristic_android =
      static_cast<BluetoothRemoteGattCharacteristicAndroid*>(characteristic);
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_FakeBluetoothGattCharacteristic_addDescriptor(
      env, characteristic_android->GetJavaObject(),
      base::android::ConvertUTF8ToJavaString(env, uuid));
}

void BluetoothTestAndroid::RememberDescriptorForSubsequentAction(
    BluetoothRemoteGattDescriptor* descriptor) {
  BluetoothRemoteGattDescriptorAndroid* descriptor_android =
      static_cast<BluetoothRemoteGattDescriptorAndroid*>(descriptor);

  Java_FakeBluetoothGattDescriptor_rememberDescriptorForSubsequentAction(
      base::android::AttachCurrentThread(),
      descriptor_android->GetJavaObject());
}

void BluetoothTestAndroid::SimulateGattDescriptorRead(
    BluetoothRemoteGattDescriptor* descriptor,
    const std::vector<uint8_t>& value) {
  BluetoothRemoteGattDescriptorAndroid* descriptor_android =
      static_cast<BluetoothRemoteGattDescriptorAndroid*>(descriptor);
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_FakeBluetoothGattDescriptor_valueRead(
      env, descriptor_android ? descriptor_android->GetJavaObject() : nullptr,
      0,  // android.bluetooth.BluetoothGatt.GATT_SUCCESS
      base::android::ToJavaByteArray(env, value));
}

void BluetoothTestAndroid::SimulateGattDescriptorReadError(
    BluetoothRemoteGattDescriptor* descriptor,
    BluetoothGattService::GattErrorCode error_code) {
  BluetoothRemoteGattDescriptorAndroid* descriptor_android =
      static_cast<BluetoothRemoteGattDescriptorAndroid*>(descriptor);
  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<uint8_t> empty_value;

  Java_FakeBluetoothGattDescriptor_valueRead(
      env, descriptor_android->GetJavaObject(),
      BluetoothRemoteGattServiceAndroid::GetAndroidErrorCode(error_code),
      base::android::ToJavaByteArray(env, empty_value));
}

void BluetoothTestAndroid::SimulateGattDescriptorReadWillFailSynchronouslyOnce(
    BluetoothRemoteGattDescriptor* descriptor) {
  BluetoothRemoteGattDescriptorAndroid* descriptor_android =
      static_cast<BluetoothRemoteGattDescriptorAndroid*>(descriptor);
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_FakeBluetoothGattDescriptor_setReadDescriptorWillFailSynchronouslyOnce(
      env, descriptor_android->GetJavaObject());
}

void BluetoothTestAndroid::SimulateGattDescriptorWrite(
    BluetoothRemoteGattDescriptor* descriptor) {
  BluetoothRemoteGattDescriptorAndroid* descriptor_android =
      static_cast<BluetoothRemoteGattDescriptorAndroid*>(descriptor);
  Java_FakeBluetoothGattDescriptor_valueWrite(
      base::android::AttachCurrentThread(),
      descriptor_android ? descriptor_android->GetJavaObject() : nullptr,
      0);  // android.bluetooth.BluetoothGatt.GATT_SUCCESS
}

void BluetoothTestAndroid::SimulateGattDescriptorWriteError(
    BluetoothRemoteGattDescriptor* descriptor,
    BluetoothGattService::GattErrorCode error_code) {
  BluetoothRemoteGattDescriptorAndroid* descriptor_android =
      static_cast<BluetoothRemoteGattDescriptorAndroid*>(descriptor);
  Java_FakeBluetoothGattDescriptor_valueWrite(
      base::android::AttachCurrentThread(), descriptor_android->GetJavaObject(),
      BluetoothRemoteGattServiceAndroid::GetAndroidErrorCode(error_code));
}

void BluetoothTestAndroid::SimulateGattDescriptorWriteWillFailSynchronouslyOnce(
    BluetoothRemoteGattDescriptor* descriptor) {
  BluetoothRemoteGattDescriptorAndroid* descriptor_android =
      static_cast<BluetoothRemoteGattDescriptorAndroid*>(descriptor);
  Java_FakeBluetoothGattDescriptor_setWriteDescriptorWillFailSynchronouslyOnce(
      base::android::AttachCurrentThread(),
      descriptor_android->GetJavaObject());
}

void BluetoothTestAndroid::FailNextServiceConnection(
    BluetoothDevice* device,
    const std::string& error_message) {
  BluetoothDeviceAndroid* device_android =
      static_cast<BluetoothDeviceAndroid*>(device);
  Java_FakeBluetoothDevice_failNextServiceConnection(
      base::android::AttachCurrentThread(), device_android->GetJavaObject(),
      error_message);
}

std::vector<uint8_t> BluetoothTestAndroid::GetSentBytes(
    BluetoothSocket* socket) {
  BluetoothSocketAndroid* socket_android =
      static_cast<BluetoothSocketAndroid*>(socket);
  base::android::ScopedJavaLocalRef<jbyteArray> j_sent_bytes =
      Java_FakeBluetoothSocket_getSentBytes(
          base::android::AttachCurrentThread(),
          socket_android->GetJavaObject());
  std::vector<uint8_t> sent_bytes;
  base::android::JavaByteArrayToByteVector(base::android::AttachCurrentThread(),
                                           j_sent_bytes, &sent_bytes);
  return sent_bytes;
}

void BluetoothTestAndroid::SetReceivedBytes(BluetoothSocket* socket,
                                            const std::vector<uint8_t>& bytes) {
  BluetoothSocketAndroid* socket_android =
      static_cast<BluetoothSocketAndroid*>(socket);
  base::android::ScopedJavaLocalRef<jbyteArray> j_bytes =
      base::android::ToJavaByteArray(base::android::AttachCurrentThread(),
                                     bytes);
  Java_FakeBluetoothSocket_setReceivedBytes(
      base::android::AttachCurrentThread(), socket_android->GetJavaObject(),
      j_bytes);
}

void BluetoothTestAndroid::FailNextOperation(BluetoothSocket* socket,
                                             const std::string& error_message) {
  BluetoothSocketAndroid* socket_android =
      static_cast<BluetoothSocketAndroid*>(socket);
  Java_FakeBluetoothSocket_setNextOperationExceptionMessage(
      base::android::AttachCurrentThread(), socket_android->GetJavaObject(),
      error_message);
}

void BluetoothTestAndroid::OnFakeBluetoothDeviceConnectGattCalled(JNIEnv* env) {
  gatt_open_connections_++;
  gatt_connection_attempts_++;
}

void BluetoothTestAndroid::OnFakeBluetoothGattDisconnect(JNIEnv* env) {
  gatt_disconnection_attempts_++;
}

void BluetoothTestAndroid::OnFakeBluetoothGattClose(JNIEnv* env) {
  gatt_open_connections_--;

  // close implies disconnect
  gatt_disconnection_attempts_++;
}

void BluetoothTestAndroid::OnFakeBluetoothGattDiscoverServices(JNIEnv* env) {
  gatt_discovery_attempts_++;
}

void BluetoothTestAndroid::OnFakeBluetoothGattSetCharacteristicNotification(
    JNIEnv* env) {
  gatt_notify_characteristic_attempts_++;
}

void BluetoothTestAndroid::OnFakeBluetoothGattReadCharacteristic(JNIEnv* env) {
  gatt_read_characteristic_attempts_++;
}

void BluetoothTestAndroid::OnFakeBluetoothGattWriteCharacteristic(
    JNIEnv* env,
    const JavaParamRef<jbyteArray>& value) {
  gatt_write_characteristic_attempts_++;
  base::android::JavaByteArrayToByteVector(env, value, &last_write_value_);
}

void BluetoothTestAndroid::OnFakeBluetoothGattReadDescriptor(JNIEnv* env) {
  gatt_read_descriptor_attempts_++;
}

void BluetoothTestAndroid::OnFakeBluetoothGattWriteDescriptor(
    JNIEnv* env,
    const JavaParamRef<jbyteArray>& value) {
  gatt_write_descriptor_attempts_++;
  base::android::JavaByteArrayToByteVector(env, value, &last_write_value_);
}

void BluetoothTestAndroid::OnFakeAdapterStateChanged(
    JNIEnv* env,
    const bool powered) {
  // Delegate to the real implementation if the adapter is still alive.
  if (adapter_) {
    static_cast<BluetoothAdapterAndroid*>(adapter_.get())
        ->OnAdapterStateChanged(
            env, base::android::JavaParamRef<jobject>(nullptr), powered);
  }
}

}  // namespace device
