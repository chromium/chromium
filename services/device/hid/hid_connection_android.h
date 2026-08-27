// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_HID_HID_CONNECTION_ANDROID_H_
#define SERVICES_DEVICE_HID_HID_CONNECTION_ANDROID_H_

#include <cstdint>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/containers/flat_map.h"
#include "base/sequence_checker.h"
#include "services/device/hid/hid_connection.h"

namespace device {

class HidConnectionAndroid : public HidConnection {
 public:
  HidConnectionAndroid(
      scoped_refptr<HidDeviceInfo> device_info,
      bool allow_protected_reports,
      bool allow_fido_reports,
      base::android::ScopedJavaGlobalRef<jobject> j_connection);

  HidConnectionAndroid(const HidConnectionAndroid&) = delete;
  HidConnectionAndroid& operator=(const HidConnectionAndroid&) = delete;

  // Called by JNI.
  void OnWriteComplete(uint32_t callback_id, bool success);
  void OnReadFeatureComplete(uint32_t callback_id,
                             bool success,
                             uint32_t report_id,
                             const std::vector<uint8_t>& data);
  void OnInputReport(JNIEnv* env,
                     uint8_t report_id,
                     const base::android::JavaRef<jbyteArray>& data);

 private:
  ~HidConnectionAndroid() override;

  // HidConnection:
  void PlatformClose() override;
  void PlatformWrite(scoped_refptr<base::RefCountedBytes> buffer,
                     WriteCallback callback) override;
  void PlatformGetFeatureReport(uint8_t report_id,
                                ReadCallback callback) override;
  void PlatformSendFeatureReport(scoped_refptr<base::RefCountedBytes> buffer,
                                 WriteCallback callback) override;

  // Java object org.chromium.device.hid.ChromeHidConnection.
  const base::android::ScopedJavaGlobalRef<jobject> j_connection_;

  uint32_t next_callback_id_ = 0;
  base::flat_map<uint32_t, WriteCallback> pending_writes_;
  base::flat_map<uint32_t, ReadCallback> pending_feature_reads_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace device

#endif  // SERVICES_DEVICE_HID_HID_CONNECTION_ANDROID_H_
