// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_android.h"

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "device/bluetooth/android/wrappers.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_device_android.h"
#include "device/bluetooth/bluetooth_discovery_session_outcome.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "device/bluetooth/jni_headers/ChromeBluetoothAdapter_jni.h"
#include "device/bluetooth/jni_headers/ChromeBluetoothScanFilterBuilder_jni.h"
#include "device/bluetooth/jni_headers/ChromeBluetoothScanFilterList_jni.h"

using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaArrayOfByteArrayToBytesVector;
using base::android::JavaByteArrayToByteVector;
using base::android::JavaIntArrayToIntVector;
using base::android::JavaParamRef;
using base::android::JavaRef;

namespace {
// The poll interval in ms when there is no active discovery. This
// matches the max allowed advertisting interval for connectable
// devices.
enum { kPassivePollInterval = 11000 };
// The poll interval in ms when there is an active discovery.
enum { kActivePollInterval = 1000 };
// The delay in ms to wait before purging devices when a scan starts.
enum { kPurgeDelay = 500 };
}  // namespace

namespace device {

// static
scoped_refptr<BluetoothAdapter> BluetoothAdapter::CreateAdapter() {
  return BluetoothAdapterAndroid::Create(
      BluetoothAdapterWrapper_CreateWithDefaultAdapter());
}

// static
scoped_refptr<BluetoothAdapterAndroid> BluetoothAdapterAndroid::Create(
    const JavaRef<jobject>&
        bluetooth_adapter_wrapper) {  // Java Type: BluetoothAdapterWrapper
  auto adapter = base::WrapRefCounted(new BluetoothAdapterAndroid());

  adapter->j_adapter_.Reset(Java_ChromeBluetoothAdapter_create(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(adapter.get()),
      bluetooth_adapter_wrapper));

  adapter->ui_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();

  return adapter;
}

void BluetoothAdapterAndroid::Initialize(base::OnceClosure callback) {
  std::move(callback).Run();
}

std::string BluetoothAdapterAndroid::GetAddress() const {
  return ConvertJavaStringToUTF8(Java_ChromeBluetoothAdapter_getAddress(
      AttachCurrentThread(), j_adapter_));
}

std::string BluetoothAdapterAndroid::GetName() const {
  return ConvertJavaStringToUTF8(
      Java_ChromeBluetoothAdapter_getName(AttachCurrentThread(), j_adapter_));
}

void BluetoothAdapterAndroid::SetName(const std::string& name,
                                      base::OnceClosure callback,
                                      ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterAndroid::IsInitialized() const {
  return true;
}

bool BluetoothAdapterAndroid::IsPresent() const {
  return Java_ChromeBluetoothAdapter_isPresent(AttachCurrentThread(),
                                               j_adapter_);
}

bool BluetoothAdapterAndroid::IsPowered() const {
  return Java_ChromeBluetoothAdapter_isPowered(AttachCurrentThread(),
                                               j_adapter_);
}

bool BluetoothAdapterAndroid::IsDiscoverable() const {
  return Java_ChromeBluetoothAdapter_isDiscoverable(AttachCurrentThread(),
                                                    j_adapter_);
}

void BluetoothAdapterAndroid::SetDiscoverable(bool discoverable,
                                              base::OnceClosure callback,
                                              ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterAndroid::IsDiscovering() const {
  return Java_ChromeBluetoothAdapter_isDiscovering(AttachCurrentThread(),
                                                   j_adapter_);
}

BluetoothAdapter::UUIDList BluetoothAdapterAndroid::GetUUIDs() const {
  NOTIMPLEMENTED();
  return UUIDList();
}

void BluetoothAdapterAndroid::CreateRfcommService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    CreateServiceCallback callback,
    CreateServiceErrorCallback error_callback) {
  NOTIMPLEMENTED();
  std::move(error_callback).Run("Not Implemented");
}

void BluetoothAdapterAndroid::CreateL2capService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    CreateServiceCallback callback,
    CreateServiceErrorCallback error_callback) {
  NOTIMPLEMENTED();
  std::move(error_callback).Run("Not Implemented");
}

void BluetoothAdapterAndroid::RegisterAdvertisement(
    std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
    CreateAdvertisementCallback callback,
    AdvertisementErrorCallback error_callback) {
  std::move(error_callback)
      .Run(BluetoothAdvertisement::ERROR_UNSUPPORTED_PLATFORM);
}

BluetoothLocalGattService* BluetoothAdapterAndroid::GetGattService(
    const std::string& identifier) const {
  return nullptr;
}

void BluetoothAdapterAndroid::OnAdapterStateChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller,
    const bool powered) {
  RunPendingPowerCallbacks();
  NotifyAdapterPoweredChanged(powered);
}

void BluetoothAdapterAndroid::OnScanFailed(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller) {
  MarkDiscoverySessionsAsInactive();
}

void BluetoothAdapterAndroid::CreateOrUpdateDeviceOnScan(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller,
    const JavaParamRef<jstring>& address,
    const JavaParamRef<jobject>&
        bluetooth_device_wrapper,  // Java Type: bluetoothDeviceWrapper
    const JavaParamRef<jstring>& local_name,
    int32_t rssi,
    const JavaParamRef<jobjectArray>& advertised_uuids,  // Java Type: String[]
    int32_t tx_power,
    const JavaParamRef<jobjectArray>& service_data_keys,  // Java Type: String[]
    const JavaParamRef<jobjectArray>& service_data_values,  // Java Type: byte[]
    const JavaParamRef<jintArray>& manufacturer_data_keys,  // Java Type: int[]
    const JavaParamRef<jobjectArray>&
        manufacturer_data_values,  // Java Type: byte[]
    int32_t advertisement_flags) {
  std::string device_address = ConvertJavaStringToUTF8(env, address);
  auto iter = devices_.find(device_address);

  bool is_new_device = false;
  std::unique_ptr<BluetoothDeviceAndroid> device_android_owner;
  BluetoothDeviceAndroid* device_android;

  if (iter == devices_.end()) {
    // New device.
    is_new_device = true;
    device_android_owner =
        BluetoothDeviceAndroid::Create(this, bluetooth_device_wrapper);
    device_android = device_android_owner.get();
  } else {
    // Existing device.
    device_android = static_cast<BluetoothDeviceAndroid*>(iter->second.get());
  }
  DCHECK(device_android);

  std::vector<std::string> advertised_uuids_strings;
  AppendJavaStringArrayToStringVector(env, advertised_uuids,
                                      &advertised_uuids_strings);
  BluetoothDevice::UUIDList advertised_bluetooth_uuids;
  for (std::string& uuid : advertised_uuids_strings) {
    advertised_bluetooth_uuids.push_back(BluetoothUUID(std::move(uuid)));
  }

  std::vector<std::string> service_data_keys_vector;
  std::vector<std::vector<uint8_t>> service_data_values_vector;
  AppendJavaStringArrayToStringVector(env, service_data_keys,
                                      &service_data_keys_vector);
  JavaArrayOfByteArrayToBytesVector(env, service_data_values,
                                    &service_data_values_vector);
  BluetoothDeviceAndroid::ServiceDataMap service_data_map;
  for (size_t i = 0; i < service_data_keys_vector.size(); i++) {
    service_data_map.insert({BluetoothUUID(service_data_keys_vector[i]),
                             service_data_values_vector[i]});
  }

  std::vector<jint> manufacturer_data_keys_vector;
  std::vector<std::vector<uint8_t>> manufacturer_data_values_vector;
  JavaIntArrayToIntVector(env, manufacturer_data_keys,
                          &manufacturer_data_keys_vector);
  JavaArrayOfByteArrayToBytesVector(env, manufacturer_data_values,
                                    &manufacturer_data_values_vector);
  BluetoothDeviceAndroid::ManufacturerDataMap manufacturer_data_map;
  for (size_t i = 0; i < manufacturer_data_keys_vector.size(); i++) {
    manufacturer_data_map.insert(
        {static_cast<uint16_t>(manufacturer_data_keys_vector[i]),
         manufacturer_data_values_vector[i]});
  }

  int8_t clamped_tx_power = BluetoothDevice::ClampPower(tx_power);

  device_android->UpdateAdvertisementData(
      BluetoothDevice::ClampPower(rssi),
      // Android uses -1 to indicate no advertising flags.
      // https://developer.android.com/reference/android/bluetooth/le/ScanRecord.html#getAdvertiseFlags()
      advertisement_flags == -1 ? std::nullopt
                                : std::make_optional(advertisement_flags),
      advertised_bluetooth_uuids,
      // Android uses INT32_MIN to indicate no Advertised Tx Power.
      // https://developer.android.com/reference/android/bluetooth/le/ScanRecord.html#getTxPowerLevel()
      tx_power == INT32_MIN ? std::nullopt
                            : std::make_optional(clamped_tx_power),
      service_data_map, manufacturer_data_map);

  for (auto& observer : observers_) {
    std::optional<std::string> device_name_opt = device_android->GetName();
    std::optional<std::string> advertisement_name_opt;
    if (local_name)
      advertisement_name_opt = ConvertJavaStringToUTF8(env, local_name);

    observer.DeviceAdvertisementReceived(
        device_android->GetAddress(), device_name_opt, advertisement_name_opt,
        BluetoothDevice::ClampPower(rssi),
        // Android uses INT32_MIN to indicate no Advertised Tx Power.
        // https://developer.android.com/reference/android/bluetooth/le/ScanRecord.html#getTxPowerLevel()
        tx_power == INT32_MIN ? std::nullopt
                              : std::make_optional(clamped_tx_power),
        std::nullopt, /* TODO(crbug.com/41240161) Implement appearance */
        advertised_bluetooth_uuids, service_data_map, manufacturer_data_map);
  }

  if (is_new_device) {
    devices_[device_address] = std::move(device_android_owner);
    for (auto& observer : observers_)
      observer.DeviceAdded(this, device_android);
  } else {
    for (auto& observer : observers_)
      observer.DeviceChanged(this, device_android);
  }
}

BluetoothAdapterAndroid::BluetoothAdapterAndroid() {}

BluetoothAdapterAndroid::~BluetoothAdapterAndroid() {
  Java_ChromeBluetoothAdapter_onBluetoothAdapterAndroidDestruction(
      AttachCurrentThread(), j_adapter_);
}

void BluetoothAdapterAndroid::PurgeTimedOutDevices() {
  RemoveTimedOutDevices();
  if (IsDiscovering()) {
    ui_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&BluetoothAdapterAndroid::PurgeTimedOutDevices,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Milliseconds(kActivePollInterval));
  } else {
    ui_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&BluetoothAdapterAndroid::RemoveTimedOutDevices,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Milliseconds(kPassivePollInterval));
  }
}

base::WeakPtr<BluetoothAdapter> BluetoothAdapterAndroid::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool BluetoothAdapterAndroid::SetPoweredImpl(bool powered) {
  return Java_ChromeBluetoothAdapter_setPowered(AttachCurrentThread(),
                                                j_adapter_, powered);
}

void BluetoothAdapterAndroid::UpdateFilter(
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
    DiscoverySessionResultCallback callback) {
  // If there is only 1 discovery session then StartScan should be called and
  // not UpdateFilter.
  DCHECK_GT(NumDiscoverySessions(), 1);
  if (IsPowered()) {
    // TODO(jameshollyer): Actually update the filter in Android.
    std::move(callback).Run(/*is_error=*/false,
                            UMABluetoothDiscoverySessionOutcome::SUCCESS);
    return;
  } else {
    DVLOG(1) << "UpdateFilter: Fails: !isPowered";
    std::move(callback).Run(/*is_error=*/true,
                            UMABluetoothDiscoverySessionOutcome::UNKNOWN);
  }
}

base::android::ScopedJavaLocalRef<jobject>
BluetoothAdapterAndroid::CreateAndroidFilter(
    const BluetoothDiscoveryFilter* discovery_filter) {
  base::android::ScopedJavaLocalRef<jobject> android_filters =
      Java_ChromeBluetoothScanFilterList_create(AttachCurrentThread());
  const base::flat_set<device::BluetoothDiscoveryFilter::DeviceInfoFilter>*
      device_filters = discovery_filter->GetDeviceFilters();
  for (const auto& device_filter : *device_filters) {
    base::android::ScopedJavaLocalRef<jobject> filter_builder =
        Java_ChromeBluetoothScanFilterBuilder_create(AttachCurrentThread());
    if (!device_filter.uuids.empty()) {
      // Set the service UUID to the first UUID in the list because Android does
      // not support filtering for multiple UUIDs. This will return a superset
      // of the devices that advertise all UUIDs in the list and it will be
      // filtered internally when returned.
      Java_ChromeBluetoothScanFilterBuilder_setServiceUuid(
          AttachCurrentThread(), filter_builder,
          base::android::ConvertUTF8ToJavaString(
              AttachCurrentThread(), device_filter.uuids.begin()->value()));
    }
    if (!device_filter.name.empty()) {
      Java_ChromeBluetoothScanFilterBuilder_setDeviceName(
          AttachCurrentThread(), filter_builder,
          base::android::ConvertUTF8ToJavaString(AttachCurrentThread(),
                                                 device_filter.name));
    }
    base::android::ScopedJavaLocalRef<jobject> scan_filter =
        Java_ChromeBluetoothScanFilterBuilder_build(AttachCurrentThread(),
                                                    filter_builder);
    Java_ChromeBluetoothScanFilterList_addFilter(AttachCurrentThread(),
                                                 android_filters, scan_filter);
  }

  return Java_ChromeBluetoothScanFilterList_getList(AttachCurrentThread(),
                                                    android_filters);
}

void BluetoothAdapterAndroid::StartScanWithFilter(
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
    DiscoverySessionResultCallback callback) {
  // This function should only be called if this is the first discovery session.
  // Otherwise we should have called updateFilter.
  DCHECK_EQ(NumDiscoverySessions(), 1);
  bool session_added = false;
  if (IsPowered()) {
    auto android_scan_filter = CreateAndroidFilter(discovery_filter.get());
    if (Java_ChromeBluetoothAdapter_startScan(AttachCurrentThread(), j_adapter_,
                                              android_scan_filter)) {
      session_added = true;

      // Using a delayed task in order to give the adapter some time
      // to settle before purging devices.
      ui_task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&BluetoothAdapterAndroid::PurgeTimedOutDevices,
                         weak_ptr_factory_.GetWeakPtr()),
          base::Milliseconds(kPurgeDelay));
    }
  } else {
    DVLOG(1) << "StartScanWithFilter: Fails: !isPowered";
  }

  if (session_added) {
    DVLOG(1) << "StartScanWithFilter: Now " << unsigned(NumDiscoverySessions())
             << " sessions.";
    std::move(callback).Run(/*is_error=*/false,
                            UMABluetoothDiscoverySessionOutcome::SUCCESS);
  } else {
    // TODO(scheib): Eventually wire the SCAN_FAILED result through to here.
    std::move(callback).Run(/*is_error=*/true,
                            UMABluetoothDiscoverySessionOutcome::UNKNOWN);
  }
}

void BluetoothAdapterAndroid::StopScan(
    DiscoverySessionResultCallback callback) {
  DCHECK(NumDiscoverySessions() == 0);

  DVLOG(1) << "Stopping scan.";
  if (Java_ChromeBluetoothAdapter_stopScan(AttachCurrentThread(), j_adapter_)) {
    std::move(callback).Run(/*is_error=*/false,
                            UMABluetoothDiscoverySessionOutcome::SUCCESS);
  } else {
    // TODO(scheib): Eventually wire the SCAN_FAILED result through to here.
    std::move(callback).Run(/*is_error=*/true,
                            UMABluetoothDiscoverySessionOutcome::UNKNOWN);
  }
  for (const auto& device_id_object_pair : devices_)
    device_id_object_pair.second->ClearAdvertisementData();
}

void BluetoothAdapterAndroid::RemovePairingDelegateInternal(
    device::BluetoothDevice::PairingDelegate* pairing_delegate) {}

}  // namespace device
