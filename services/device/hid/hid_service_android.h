// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_HID_HID_SERVICE_ANDROID_H_
#define SERVICES_DEVICE_HID_HID_SERVICE_ANDROID_H_

#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "services/device/hid/hid_service.h"

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
  void OnDeviceAdded(int32_t device_id,
                     int32_t vendor_id,
                     int32_t product_id,
                     const std::string& product_name,
                     const std::string& serial_number,
                     const std::string& physical_address,
                     int32_t transport_type,
                     const std::vector<uint8_t>& report_descriptor);
  void OnDeviceRemoved(int32_t device_id);
  void OnEnumerationComplete();
  void OnAllDevicesRemoved();

 private:
  // HidService implementation:
  void Connect(const std::string& device_guid,
               bool allow_protected_reports,
               bool allow_fido_reports,
               ConnectCallback callback) override;
  base::WeakPtr<HidService> GetWeakPtr() override;

  base::android::ScopedJavaGlobalRef<jobject> obj_;
  base::WeakPtrFactory<HidServiceAndroid> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_HID_HID_SERVICE_ANDROID_H_
