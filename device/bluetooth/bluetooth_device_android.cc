// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device_android.h"

#include <jni.h>

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/stl_util.h"
#include "base/task/sequenced_task_runner.h"
#include "device/base/features.h"
#include "device/bluetooth/android/outcome.h"
#include "device/bluetooth/bluetooth_adapter_android.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_android.h"
#include "device/bluetooth/bluetooth_socket_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "device/bluetooth/jni_headers/ChromeBluetoothDevice_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::JavaRef;

namespace device {

class BluetoothSocketThread;

std::unique_ptr<BluetoothDeviceAndroid> BluetoothDeviceAndroid::Create(
    BluetoothAdapterAndroid* adapter,
    const JavaRef<jobject>&
        bluetooth_device_wrapper,  // Java Type: bluetoothDeviceWrapper
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<BluetoothSocketThread> socket_thread) {
  std::unique_ptr<BluetoothDeviceAndroid> device(
      new BluetoothDeviceAndroid(adapter, task_runner, socket_thread));

  device->j_device_.Reset(Java_ChromeBluetoothDevice_create(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(device.get()),
      bluetooth_device_wrapper));

  device->LoadInitialCachedMetadata();

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

void BluetoothDeviceAndroid::LoadInitialCachedMetadata() {
  CHECK(adapter_->IsPowered());

  // We call a few getters in which cached metadata are updated.
  GetName();
  GetAddress();
  GetUUIDs();
  GetBluetoothClass();
  IsPaired();
}

uint32_t BluetoothDeviceAndroid::GetBluetoothClass() const {
  if (adapter_->IsPowered()) {
    cached_class_ = Java_ChromeBluetoothDevice_getBluetoothClass(
        AttachCurrentThread(), j_device_);
  }
  return cached_class_;
}

BluetoothTransport BluetoothDeviceAndroid::GetType() const {
  if (adapter_->IsPowered()) {
    // Device types in Android BluetoothDevice share the same value as
    // BluetoothTransport.
    cached_type_ = static_cast<BluetoothTransport>(
        Java_ChromeBluetoothDevice_getType(AttachCurrentThread(), j_device_));
  }
  return cached_type_;
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
  if (adapter_->IsPowered()) {
    auto name =
        Java_ChromeBluetoothDevice_getName(AttachCurrentThread(), j_device_);
    if (name.is_null()) {
      cached_name_.reset();
    } else {
      cached_name_ = ConvertJavaStringToUTF8(name);
    }
  }
  return cached_name_;
}

bool BluetoothDeviceAndroid::IsPaired() const {
  if (adapter_->IsPowered()) {
    cached_paired_ =
        Java_ChromeBluetoothDevice_isPaired(AttachCurrentThread(), j_device_);
  }
  return cached_paired_;
}

bool BluetoothDeviceAndroid::IsConnected() const {
  return IsGattConnected() || (connected_transport_ && adapter_->IsPowered());
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

BluetoothDevice::UUIDSet BluetoothDeviceAndroid::GetUUIDs() const {
  if (!base::FeatureList::IsEnabled(features::kBluetoothRfcommAndroid)) {
    return BluetoothDevice::GetUUIDs();
  }

  BluetoothTransport device_type = GetType();
  if (device_type == BLUETOOTH_TRANSPORT_LE ||
      device_type == BLUETOOTH_TRANSPORT_INVALID) {
    return BluetoothDevice::GetUUIDs();
  }

  if (adapter_->IsPowered()) {
    // Java type: String[]
    base::android::ScopedJavaLocalRef<jobjectArray> sdp_uuids =
        Java_ChromeBluetoothDevice_getUuids(AttachCurrentThread(), j_device_);
    std::vector<std::string> sdp_uuid_strings;
    base::android::AppendJavaStringArrayToStringVector(
        AttachCurrentThread(), sdp_uuids, &sdp_uuid_strings);
    for (std::string& uuid : sdp_uuid_strings) {
      cached_sdp_uuids_.insert(BluetoothUUID(std::move(uuid)));
    }
  }

  if (device_type == BLUETOOTH_TRANSPORT_CLASSIC) {
    return cached_sdp_uuids_;
  }

  // Dual transport device
  return base::STLSetUnion<BluetoothDevice::UUIDSet>(
      cached_sdp_uuids_, BluetoothDevice::GetUUIDs());
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
  Outcome outcome(Java_ChromeBluetoothDevice_connectToService(
      AttachCurrentThread(), j_device_, uuid.canonical_value()));
  if (!outcome) {
    std::move(error_callback).Run(outcome.GetExceptionMessage());
    return;
  }

  scoped_refptr<BluetoothSocketAndroid> socket = BluetoothSocketAndroid::Create(
      outcome.GetResult(), ui_task_runner_, socket_thread_);
  socket->Connect(base::BindOnce(std::move(callback), socket),
                  std::move(error_callback));
}

void BluetoothDeviceAndroid::ConnectToServiceInsecurely(
    const BluetoothUUID& uuid,
    ConnectToServiceCallback callback,
    ConnectToServiceErrorCallback error_callback) {
  Outcome outcome(Java_ChromeBluetoothDevice_connectToServiceInsecurely(
      AttachCurrentThread(), j_device_, uuid.canonical_value()));
  if (!outcome) {
    std::move(error_callback).Run(outcome.GetExceptionMessage());
    return;
  }

  scoped_refptr<BluetoothSocketAndroid> socket = BluetoothSocketAndroid::Create(
      outcome.GetResult(), ui_task_runner_, socket_thread_);
  socket->Connect(base::BindOnce(std::move(callback), socket),
                  std::move(error_callback));
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

BluetoothDeviceAndroid::BluetoothDeviceAndroid(
    BluetoothAdapterAndroid* adapter,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<BluetoothSocketThread> socket_thread)
    : BluetoothDevice(adapter),
      ui_task_runner_(ui_task_runner),
      socket_thread_(socket_thread) {}

void BluetoothDeviceAndroid::CreateGattConnectionImpl(
    std::optional<device::BluetoothUUID> service_uuid) {
  Java_ChromeBluetoothDevice_createGattConnectionImpl(AttachCurrentThread(),
                                                      j_device_);
}

void BluetoothDeviceAndroid::DisconnectGatt() {
  Java_ChromeBluetoothDevice_disconnectGatt(AttachCurrentThread(), j_device_);
}

void BluetoothDeviceAndroid::UpdateAclConnectState(uint8_t transport,
                                                   bool connected) {
  if (connected) {
    connected_transport_ |= transport;
  } else {
    connected_transport_ &= ~transport;
  }
}

}  // namespace device
