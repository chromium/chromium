// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/containers/contains.h"
#include "device/bluetooth/bluetooth_adapter_android.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "device/bluetooth/jni_headers/ChromeBluetoothDevice_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::JavaRef;

namespace device {

std::unique_ptr<BluetoothDeviceAndroid> BluetoothDeviceAndroid::Create(
    BluetoothAdapterAndroid* adapter,
    const JavaRef<jobject>&
        bluetooth_device_wrapper) {  // Java Type: bluetoothDeviceWrapper
  std::unique_ptr<BluetoothDeviceAndroid> device(
      new BluetoothDeviceAndroid(adapter));

  device->j_device_.Reset(Java_ChromeBluetoothDevice_create(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(device.get()),
      bluetooth_device_wrapper));

  return device;
}

BluetoothDeviceAndroid::~BluetoothDeviceAndroid() {
  Java_ChromeBluetoothDevice_onBluetoothDeviceAndroidDestruction(
      AttachCurrentThread(), j_device_);
}

base::android::ScopedJavaLocalRef<jobject>
BluetoothDeviceAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(j_device_);
}

uint32_t BluetoothDeviceAndroid::GetBluetoothClass() const {
  return Java_ChromeBluetoothDevice_getBluetoothClass(AttachCurrentThread(),
                                                      j_device_);
}

std::string BluetoothDeviceAndroid::GetAddress() const {
  return ConvertJavaStringToUTF8(
      Java_ChromeBluetoothDevice_getAddress(AttachCurrentThread(), j_device_));
}

BluetoothDevice::AddressType BluetoothDeviceAndroid::GetAddressType() const {
  NOTIMPLEMENTED();
  return ADDR_TYPE_UNKNOWN;
}

BluetoothDevice::VendorIDSource BluetoothDeviceAndroid::GetVendorIDSource()
    const {
  // Android API does not provide Vendor ID.
  return VENDOR_ID_UNKNOWN;
}

uint16_t BluetoothDeviceAndroid::GetVendorID() const {
  // Android API does not provide Vendor ID.
  return 0;
}

uint16_t BluetoothDeviceAndroid::GetProductID() const {
  // Android API does not provide Product ID.
  return 0;
}

uint16_t BluetoothDeviceAndroid::GetDeviceID() const {
  // Android API does not provide Device ID.
  return 0;
}

uint16_t BluetoothDeviceAndroid::GetAppearance() const {
  // TODO(crbug.com/41240161): Implementing GetAppearance()
  // on mac, win, and android platforms for chrome
  NOTIMPLEMENTED();
  return 0;
}

std::optional<std::string> BluetoothDeviceAndroid::GetName() const {
  auto name =
      Java_ChromeBluetoothDevice_getName(AttachCurrentThread(), j_device_);
  if (name.is_null())
    return std::nullopt;
  return ConvertJavaStringToUTF8(name);
}

bool BluetoothDeviceAndroid::IsPaired() const {
  return Java_ChromeBluetoothDevice_isPaired(AttachCurrentThread(), j_device_);
}

bool BluetoothDeviceAndroid::IsConnected() const {
  return IsGattConnected();
}

bool BluetoothDeviceAndroid::IsGattConnected() const {
  return gatt_connected_;
}

bool BluetoothDeviceAndroid::IsConnectable() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceAndroid::IsConnecting() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceAndroid::ExpectingPinCode() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceAndroid::ExpectingPasskey() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceAndroid::ExpectingConfirmation() const {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothDeviceAndroid::GetConnectionInfo(
    ConnectionInfoCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(ConnectionInfo());
}

void BluetoothDeviceAndroid::SetConnectionLatency(
    ConnectionLatency connection_latency,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::Connect(PairingDelegate* pairing_delegate,
                                     ConnectCallback callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::SetPinCode(const std::string& pincode) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::SetPasskey(uint32_t passkey) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::ConfirmPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::RejectPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::CancelPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::Disconnect(base::OnceClosure callback,
                                        ErrorCallback error_callback) {
  // TODO(scheib): Also update unit tests for BluetoothGattConnection.
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::Forget(base::OnceClosure callback,
                                    ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::ConnectToService(
    const BluetoothUUID& uuid,
    ConnectToServiceCallback callback,
    ConnectToServiceErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::ConnectToServiceInsecurely(
    const BluetoothUUID& uuid,
    ConnectToServiceCallback callback,
    ConnectToServiceErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::OnConnectionStateChange(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    int32_t status,
    bool connected) {
  gatt_connected_ = connected;
  if (gatt_connected_) {
    DidConnectGatt(/*error_code=*/std::nullopt);
  } else if (!create_gatt_connection_callbacks_.empty()) {
    // We assume that if there are any pending connection callbacks there
    // was a failed connection attempt.
    // TODO(ortuno): Return an error code based on |status|
    // http://crbug.com/578191
    DidConnectGatt(ERROR_FAILED);
  } else {
    // Otherwise an existing connection was terminated.
    gatt_services_.clear();
    device_uuids_.ClearServiceUUIDs();
    SetGattServicesDiscoveryComplete(false);
    DidDisconnectGatt();
  }
}

void BluetoothDeviceAndroid::OnGattServicesDiscovered(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  device_uuids_.ReplaceServiceUUIDs(gatt_services_);
  SetGattServicesDiscoveryComplete(true);
  adapter_->NotifyGattServicesDiscovered(this);
  adapter_->NotifyDeviceChanged(this);
}

void BluetoothDeviceAndroid::CreateGattRemoteService(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller,
    const JavaParamRef<jstring>& instance_id,
    const JavaParamRef<jobject>&
        bluetooth_gatt_service_wrapper) {  // BluetoothGattServiceWrapper
  std::string instance_id_string = ConvertJavaStringToUTF8(env, instance_id);

  if (base::Contains(gatt_services_, instance_id_string))
    return;

  std::unique_ptr<BluetoothRemoteGattServiceAndroid> service =
      BluetoothRemoteGattServiceAndroid::Create(GetAndroidAdapter(), this,
                                                bluetooth_gatt_service_wrapper,
                                                instance_id_string, j_device_);
  BluetoothRemoteGattServiceAndroid* service_ptr = service.get();
  gatt_services_[instance_id_string] = std::move(service);

  adapter_->NotifyGattServiceAdded(service_ptr);
}

BluetoothDeviceAndroid::BluetoothDeviceAndroid(BluetoothAdapterAndroid* adapter)
    : BluetoothDevice(adapter) {}

void BluetoothDeviceAndroid::CreateGattConnectionImpl(
    std::optional<device::BluetoothUUID> service_uuid) {
  Java_ChromeBluetoothDevice_createGattConnectionImpl(AttachCurrentThread(),
                                                      j_device_);
}

void BluetoothDeviceAndroid::DisconnectGatt() {
  Java_ChromeBluetoothDevice_disconnectGatt(AttachCurrentThread(), j_device_);
}

}  // namespace device
