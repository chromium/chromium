// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/android_device_management_error_details.h"

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/check.h"
#include "base/notimplemented.h"

namespace gaia {

AndroidDeviceManagementErrorDetails::AndroidDeviceManagementErrorDetails(
    base::android::ScopedJavaGlobalRef<jobject> java_ref)
    : java_ref_(std::move(java_ref)) {
  CHECK(java_ref_);
}

AndroidDeviceManagementErrorDetails::~AndroidDeviceManagementErrorDetails() =
    default;

std::unique_ptr<DeviceManagementErrorDetails>
AndroidDeviceManagementErrorDetails::Clone() const {
  return std::make_unique<AndroidDeviceManagementErrorDetails>(java_ref_);
}

bool AndroidDeviceManagementErrorDetails::Equals(
    const DeviceManagementErrorDetails& other) const {
  const auto* other_ptr =
      static_cast<const AndroidDeviceManagementErrorDetails*>(&other);
  // Checks strict object identity rather than semantic equality, which is
  // sufficient as this is mainly used for unit tests.
  return base::android::AttachCurrentThread()->IsSameObject(
      java_ref_.obj(), other_ptr->java_ref_.obj());
}

bool AndroidDeviceManagementErrorDetails::IsUserActionable() const {
  // TODO(crbug.com/566051885): Consider all errors as actionable while waiting
  // for the GMS API.
  NOTIMPLEMENTED();
  return true;
}

}  // namespace gaia
