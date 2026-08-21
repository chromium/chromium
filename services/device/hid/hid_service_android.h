// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_HID_HID_SERVICE_ANDROID_H_
#define SERVICES_DEVICE_HID_HID_SERVICE_ANDROID_H_

#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "services/device/hid/hid_service.h"

namespace base {
class SequencedTaskRunner;
}

namespace device {

// Stub implementation of HidService for Android. Future CLs will connect this
// to Android OS USB/HID APIs via JNI.
class HidServiceAndroid : public HidService {
 public:
  HidServiceAndroid();
  HidServiceAndroid(const HidServiceAndroid&) = delete;
  HidServiceAndroid& operator=(const HidServiceAndroid&) = delete;
  ~HidServiceAndroid() override;

  // Called from Java via JNI:
  void OnDeviceAdded(const base::android::JavaRef<jobject>& j_device,
                     int32_t device_id,
                     int32_t vendor_id,
                     int32_t product_id,
                     const std::string& product_name,
                     const std::string& serial_number,
                     const std::string& physical_address,
                     int32_t transport_type,
                     const std::vector<uint8_t>& report_descriptor);
  void OnDeviceRemoved(int32_t device_id);
  void OnConnectComplete(int32_t callback_id,
                         const base::android::JavaRef<jobject>& j_connection);
  void OnEnumerationComplete();
  void OnAllDevicesRemoved();

 private:
  using ConnectHelperCallback =
      base::OnceCallback<void(const base::android::JavaRef<jobject>&)>;

  // HidService implementation:
  void Connect(const std::string& device_guid,
               bool allow_protected_reports,
               bool allow_fido_reports,
               ConnectCallback callback) override;
  base::WeakPtr<HidService> GetWeakPtr() override;

  void CreateConnectionAndRunCallback(
      scoped_refptr<HidDeviceInfo> device_info,
      bool allow_protected_reports,
      bool allow_fido_reports,
      ConnectCallback callback,
      const base::android::JavaRef<jobject>& j_connection);

  base::android::ScopedJavaGlobalRef<jobject> obj_;
  // Maps device_guid to the Java HidDevice reference.
  base::flat_map<std::string, base::android::ScopedJavaGlobalRef<jobject>>
      java_devices_by_guid_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  uint32_t next_connect_callback_id_ = 0;
  base::flat_map<uint32_t, ConnectHelperCallback> pending_connects_;
  base::WeakPtrFactory<HidServiceAndroid> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_HID_HID_SERVICE_ANDROID_H_
