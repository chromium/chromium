// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_ANDROID_DEVICE_MANAGEMENT_ERROR_DETAILS_H_
#define GOOGLE_APIS_GAIA_ANDROID_DEVICE_MANAGEMENT_ERROR_DETAILS_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/component_export.h"
#include "google_apis/gaia/device_management_error_details.h"

namespace gaia {

// Android implementation of DeviceManagementErrorDetails. It holds a reference
// to the Java DeviceManagementErrorDetails instance.
class COMPONENT_EXPORT(GOOGLE_APIS) AndroidDeviceManagementErrorDetails
    : public DeviceManagementErrorDetails {
 public:
  explicit AndroidDeviceManagementErrorDetails(
      base::android::ScopedJavaGlobalRef<jobject> java_ref);
  ~AndroidDeviceManagementErrorDetails() override;

  AndroidDeviceManagementErrorDetails(
      const AndroidDeviceManagementErrorDetails&) = delete;
  AndroidDeviceManagementErrorDetails& operator=(
      const AndroidDeviceManagementErrorDetails&) = delete;

  std::unique_ptr<DeviceManagementErrorDetails> Clone() const override;
  bool Equals(const DeviceManagementErrorDetails& other) const override;
  bool IsUserActionable() const override;

  const base::android::JavaRef<jobject>& GetJavaObject() const {
    return java_ref_;
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

}  // namespace gaia

#endif  // GOOGLE_APIS_GAIA_ANDROID_DEVICE_MANAGEMENT_ERROR_DETAILS_H_
