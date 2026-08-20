// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_service_android.h"

#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/notimplemented.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "services/device/hid/hid_connection.h"
#include "services/device/hid/hid_device_info.h"
#include "services/device/hid/jni_headers/ChromeHidManager_jni.h"
#include "third_party/jni_zero/default_conversions.h"

namespace device {

namespace {

// android.hardware.hid.HidDevice transport constants.
// Defined in Android SDK android.hardware.hid.HidDevice:
constexpr int kTransportI2c = 1;        // TRANSPORT_I2C
constexpr int kTransportUsb = 3;        // TRANSPORT_USB
constexpr int kTransportSpi = 4;        // TRANSPORT_SPI
constexpr int kTransportBluetooth = 5;  // TRANSPORT_BLUETOOTH
constexpr int kTransportUhid = 6;       // TRANSPORT_UHID

}  // namespace

HidServiceAndroid::HidServiceAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  obj_ = Java_ChromeHidManager_create(env, reinterpret_cast<intptr_t>(this));
  if (obj_) {
    Java_ChromeHidManager_enumerateDevices(env, obj_);
  } else {
    FirstEnumerationComplete();
  }
}

HidServiceAndroid::~HidServiceAndroid() {
  if (obj_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_ChromeHidManager_shutdown(env, obj_);
  }
}

void HidServiceAndroid::Connect(const std::string& device_guid,
                                bool allow_protected_reports,
                                bool allow_fido_reports,
                                ConnectCallback callback) {
  NOTIMPLEMENTED_LOG_ONCE();
  std::move(callback).Run(nullptr);
}

base::WeakPtr<HidService> HidServiceAndroid::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void HidServiceAndroid::OnDeviceAdded(
    int32_t device_id,
    int32_t vendor_id,
    int32_t product_id,
    const std::string& product_name,
    const std::string& serial_number,
    const std::string& physical_address,
    int32_t transport_type,
    const std::vector<uint8_t>& report_descriptor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string platform_device_id_str = base::NumberToString(device_id);

  mojom::HidBusType bus_type = mojom::HidBusType::kHIDBusTypeUnknown;
  switch (transport_type) {
    case kTransportUsb:
      bus_type = mojom::HidBusType::kHIDBusTypeUSB;
      break;
    case kTransportBluetooth:
      bus_type = mojom::HidBusType::kHIDBusTypeBluetooth;
      break;
    case kTransportI2c:
      bus_type = mojom::HidBusType::kHIDBusTypeI2C;
      break;
    case kTransportSpi:
      bus_type = mojom::HidBusType::kHIDBusTypeSPI;
      break;
    case kTransportUhid:
      bus_type = mojom::HidBusType::kHIDBusTypeUHID;
      break;
    default:
      bus_type = mojom::HidBusType::kHIDBusTypeUnknown;
      break;
  }

  std::string physical_device_id;
  if (transport_type == kTransportBluetooth) {
    // For Bluetooth devices, Android's getPhysicalAddress() returns the host
    // Bluetooth adapter MAC address rather than the device's address, which
    // would incorrectly cause all Bluetooth devices to share the same physical
    // device ID. Use serial_number (which contains the remote device's
    // Bluetooth MAC address) as the physical device ID instead.
    physical_device_id =
        !serial_number.empty() ? serial_number : platform_device_id_str;
  } else {
    physical_device_id =
        !physical_address.empty() ? physical_address : platform_device_id_str;
  }

  auto device_info = base::MakeRefCounted<HidDeviceInfo>(
      /*platform_device_id=*/platform_device_id_str,
      /*physical_device_id=*/physical_device_id, vendor_id, product_id,
      product_name,
      /*serial_number=*/serial_number, bus_type, report_descriptor);

  AddDevice(device_info);
}

void HidServiceAndroid::OnDeviceRemoved(int32_t device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveDevice(base::NumberToString(device_id));
}

void HidServiceAndroid::OnEnumerationComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FirstEnumerationComplete();
}

void HidServiceAndroid::OnAllDevicesRemoved() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (!devices().empty()) {
    scoped_refptr<HidDeviceInfo> device_info = devices().begin()->second;
    for (const auto& platform_id_entry :
         device_info->platform_device_id_map()) {
      RemoveDevice(platform_id_entry.platform_device_id);
    }
  }
}

DEFINE_JNI(ChromeHidManager)

}  // namespace device
